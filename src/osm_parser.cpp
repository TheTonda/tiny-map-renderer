#include "osm_parser.h"
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

std::unordered_map<std::string, std::string> extract_attrs(const std::string& s) {
    std::unordered_map<std::string, std::string> attrs;
    std::regex re(R"((\w+)="([^"]*)\")");
    auto it = std::sregex_iterator(s.begin(), s.end(), re);
    auto end = std::sregex_iterator();
    for (; it != end; ++it) {
        attrs[(*it)[1].str()] = (*it)[2].str();
    }
    return attrs;
}

std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool is_self_closing(const std::string& s) {
    auto pos = s.rfind("/>");
    return pos != std::string::npos && pos == s.size() - 2;
}

} // namespace

OSMData parse_osm_xml(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    OSMData data;

    enum class State { None, InNode, InWay, InRelation };
    State state = State::None;

    Node current_node{};
    Way current_way{};
    Relation current_relation{};

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;
        std::string trimmed = ltrim(line);
        if (trimmed.empty()) continue;

        if (starts_with(trimmed, "</node>")) {
            if (state != State::InNode) {
                throw std::runtime_error("Unexpected </node> on line " + std::to_string(line_num));
            }
            data.nodes[current_node.id] = current_node;
            state = State::None;
        } else if (starts_with(trimmed, "</way>")) {
            if (state != State::InWay) {
                throw std::runtime_error("Unexpected </way> on line " + std::to_string(line_num));
            }
            data.ways[current_way.id] = current_way;
            state = State::None;
        } else if (starts_with(trimmed, "</relation>")) {
            if (state != State::InRelation) {
                throw std::runtime_error("Unexpected </relation> on line " + std::to_string(line_num));
            }
            data.relations[current_relation.id] = current_relation;
            state = State::None;
        } else if (starts_with(trimmed, "<node")) {
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("id") || !attrs.contains("lat") || !attrs.contains("lon")) {
                throw std::runtime_error("Missing required attributes in <node> on line " + std::to_string(line_num));
            }
            Node node{};
            node.id = std::stoll(attrs["id"]);
            node.lat = std::stod(attrs["lat"]);
            node.lon = std::stod(attrs["lon"]);

            if (is_self_closing(trimmed)) {
                data.nodes[node.id] = node;
            } else {
                current_node = node;
                state = State::InNode;
            }
        } else if (starts_with(trimmed, "<way")) {
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("id")) {
                throw std::runtime_error("Missing id attribute in <way> on line " + std::to_string(line_num));
            }
            current_way = {};
            current_way.id = std::stoll(attrs["id"]);
            state = State::InWay;
        } else if (starts_with(trimmed, "<relation")) {
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("id")) {
                throw std::runtime_error("Missing id attribute in <relation> on line " + std::to_string(line_num));
            }
            current_relation = {};
            current_relation.id = std::stoll(attrs["id"]);
            state = State::InRelation;
        } else if (starts_with(trimmed, "<nd")) {
            if (state != State::InWay) {
                throw std::runtime_error("Unexpected <nd> outside <way> on line " + std::to_string(line_num));
            }
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("ref")) {
                throw std::runtime_error("Missing ref attribute in <nd> on line " + std::to_string(line_num));
            }
            current_way.node_refs.push_back(std::stoll(attrs["ref"]));
        } else if (starts_with(trimmed, "<tag")) {
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("k") || !attrs.contains("v")) {
                throw std::runtime_error("Missing k or v attribute in <tag> on line " + std::to_string(line_num));
            }
            if (state == State::InWay) {
                current_way.tags[attrs["k"]] = attrs["v"];
            } else if (state == State::InRelation) {
                current_relation.tags[attrs["k"]] = attrs["v"];
            }
        } else if (starts_with(trimmed, "<member")) {
            if (state != State::InRelation) {
                throw std::runtime_error("Unexpected <member> outside <relation> on line " + std::to_string(line_num));
            }
            auto attrs = extract_attrs(trimmed);
            if (!attrs.contains("type") || !attrs.contains("ref") || !attrs.contains("role")) {
                throw std::runtime_error("Missing attributes in <member> on line " + std::to_string(line_num));
            }
            current_relation.members.push_back({
                attrs["type"],
                std::stoll(attrs["ref"]),
                attrs["role"]
            });
        }
    }

    return data;
}
