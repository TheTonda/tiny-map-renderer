#include "osm_parser.h"
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// Streaming attribute parser: calls callback(key, value) for each key="value" pair.
// Uses memchr (SIMD-optimized) instead of regex. No heap allocations.
template<typename F>
bool parse_attrs(const char* p, const char* end, F&& callback) {
    while (p < end) {
        // Skip whitespace between attributes
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end) break;

        // Find '=' using memchr (typically SIMD-optimized)
        const char* eq = static_cast<const char*>(std::memchr(p, '=', end - p));
        if (!eq) break;

        // Key: scan backwards from '=' to find attribute name
        // Trim trailing whitespace before '='
        const char* key_end = eq;
        while (key_end > p && (*(key_end - 1) == ' ' || *(key_end - 1) == '\t')) --key_end;

        // Scan backwards to find key start (stop at ws, '<', or '/')
        const char* key_start = key_end;
        while (key_start > p
               && *(key_start - 1) != ' '
               && *(key_start - 1) != '\t'
               && *(key_start - 1) != '<'
               && *(key_start - 1) != '/') --key_start;

        if (key_start == key_end) return false; // empty key not allowed
        std::string_view key(key_start, key_end - key_start);

        // Expect =" after =
        if (eq + 1 >= end || eq[1] != '"') return false;
        const char* val_start = eq + 2;

        // Find closing quote
        const char* val_end = static_cast<const char*>(std::memchr(val_start, '"', end - val_start));
        if (!val_end) return false;
        std::string_view value(val_start, val_end - val_start);

        callback(key, value);
        p = val_end + 1;
    }
    return true;
}

// Find a single attribute by name.
// Returns the value (may be empty string for role="") or nullopt if not found.
std::optional<std::string_view> find_attr(const char* p, const char* end, std::string_view name) {
    std::optional<std::string_view> result;
    parse_attrs(p, end, [&](std::string_view k, std::string_view v) {
        if (k == name) result = v;
    });
    return result;
}


// Check if line (starting at p, len characters) starts with the given tag.
inline bool line_is(const char* p, size_t len, const char* tag) {
    size_t tag_len = std::strlen(tag);
    return len >= tag_len && std::memcmp(p, tag, tag_len) == 0;
}

// Check if this is a self-closing tag like <node ... />
inline bool is_self_closing(const char* p, size_t len) {
    return len >= 2 && p[len - 2] == '/' && p[len - 1] == '>';
}

// Skip leading whitespace, return pointer to first non-ws char
inline const char* skip_ws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

// Fast integer parse from a string_view
inline int64_t parse_int64(std::string_view sv) {
    if (sv.empty()) return 0;
    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{}) return 0;
    return val;
}

// Fast double parse from a string_view
inline double parse_double(std::string_view sv) {
    if (sv.empty()) return 0.0;
    double val = 0.0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{}) return 0.0;
    return val;
}

} // namespace

OSMData parse_osm_xml(const std::string& filepath) {
    // Memory-map the file for zero-copy line scanning (much faster than std::getline)
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        throw std::runtime_error("Failed to stat file: " + filepath);
    }

    size_t file_size = static_cast<size_t>(st.st_size);
    const char* map = static_cast<const char*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);

    if (map == MAP_FAILED) {
        throw std::runtime_error("Failed to mmap file: " + filepath);
    }

    OSMData data;

    // Pre-allocate hash maps based on file size heuristic
    if (file_size > 0) {
        data.nodes.reserve(file_size / 250);
        data.ways.reserve(file_size / 1200);
        data.relations.reserve(file_size / 50000);
    }

    enum class State { None, InNode, InWay, InRelation };
    State state = State::None;

    Node current_node{};
    Way current_way{};
    Relation current_relation{};

    const char* p = map;
    const char* map_end = map + file_size;
    int line_num = 0;

    while (p < map_end) {
        // Find end of line
        const char* line_start = p;
        const char* line_end = static_cast<const char*>(std::memchr(p, '\n', map_end - p));
        if (!line_end) line_end = map_end;
        ++line_num;

        // Find first non-whitespace character
        const char* trimmed = skip_ws(line_start, line_end);
        if (trimmed < line_end) {
            size_t len = line_end - trimmed;

            char c0 = trimmed[0];
            if (c0 == '<') {
                if (len >= 2 && trimmed[1] == '/') {
                    if (line_is(trimmed, len, "</node>")) {
                        if (state != State::InNode)
                            throw std::runtime_error("Unexpected </node> on line " + std::to_string(line_num));
                        data.nodes[current_node.id] = current_node;
                        state = State::None;
                    } else if (line_is(trimmed, len, "</way>")) {
                        if (state != State::InWay)
                            throw std::runtime_error("Unexpected </way> on line " + std::to_string(line_num));
                        data.ways[current_way.id] = current_way;
                        state = State::None;
                    } else if (line_is(trimmed, len, "</relation>")) {
                        if (state != State::InRelation)
                            throw std::runtime_error("Unexpected </relation> on line " + std::to_string(line_num));
                        data.relations[current_relation.id] = current_relation;
                        state = State::None;
                    }
                } else {
                    if (line_is(trimmed, len, "<node")) {
                        auto id_sv = find_attr(trimmed, line_end, "id");
                        auto lat_sv = find_attr(trimmed, line_end, "lat");
                        auto lon_sv = find_attr(trimmed, line_end, "lon");
                        if (!id_sv || !lat_sv || !lon_sv)
                            throw std::runtime_error("Missing required attributes in <node> on line " + std::to_string(line_num));

                        Node node{};
                        node.id = parse_int64(*id_sv);
                        node.lat = parse_double(*lat_sv);
                        node.lon = parse_double(*lon_sv);

                        if (is_self_closing(trimmed, len)) {
                            data.nodes.insert_or_assign(node.id, node);
                        } else {
                            current_node = node;
                            state = State::InNode;
                        }
                    } else if (line_is(trimmed, len, "<way")) {
                        auto id_sv = find_attr(trimmed, line_end, "id");
                        if (!id_sv)
                            throw std::runtime_error("Missing id attribute in <way> on line " + std::to_string(line_num));

                        current_way = {};
                        current_way.id = parse_int64(*id_sv);
                        state = State::InWay;
                    } else if (line_is(trimmed, len, "<relation")) {
                        auto id_sv = find_attr(trimmed, line_end, "id");
                        if (!id_sv)
                            throw std::runtime_error("Missing id attribute in <relation> on line " + std::to_string(line_num));

                        current_relation = {};
                        current_relation.id = parse_int64(*id_sv);
                        state = State::InRelation;
                    } else if (line_is(trimmed, len, "<nd")) {
                        if (state != State::InWay)
                            throw std::runtime_error("Unexpected <nd> outside <way> on line " + std::to_string(line_num));

                        auto ref_sv = find_attr(trimmed, line_end, "ref");
                        if (!ref_sv)
                            throw std::runtime_error("Missing ref attribute in <nd> on line " + std::to_string(line_num));

                        current_way.node_refs.push_back(parse_int64(*ref_sv));
                    } else if (line_is(trimmed, len, "<tag")) {
                        auto k_sv = find_attr(trimmed, line_end, "k");
                        auto v_sv = find_attr(trimmed, line_end, "v");
                        if (!k_sv || !v_sv)
                            throw std::runtime_error("Missing k or v attribute in <tag> on line " + std::to_string(line_num));

                        if (state == State::InWay) {
                            current_way.tags[std::string(*k_sv)] = std::string(*v_sv);
                        } else if (state == State::InRelation) {
                            current_relation.tags[std::string(*k_sv)] = std::string(*v_sv);
                        }
                    } else if (line_is(trimmed, len, "<member")) {
                        if (state != State::InRelation)
                            throw std::runtime_error("Unexpected <member> outside <relation> on line " + std::to_string(line_num));

                        auto type_sv = find_attr(trimmed, line_end, "type");
                        auto ref_sv = find_attr(trimmed, line_end, "ref");
                        auto role_sv = find_attr(trimmed, line_end, "role");
                        if (!type_sv || !ref_sv || !role_sv)
                            throw std::runtime_error("Missing attributes in <member> on line " + std::to_string(line_num));

                        current_relation.members.push_back({
                            std::string(*type_sv),
                            parse_int64(*ref_sv),
                            std::string(*role_sv)
                        });
                    }
                }
            }
        }

        p = line_end + 1;
        if (p > map_end) p = map_end;
    }

    munmap(const_cast<char*>(map), file_size);
    return data;
}
