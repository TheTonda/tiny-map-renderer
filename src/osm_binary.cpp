#include "osm_binary.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// Binary format (little-endian):
//
// Header:     magic[4]="TMR\0" version:u32=1
// Nodes:      count:u64, [id:i64 lat:f64 lon:f64]*
// Strtab:     count:u32, for each: len:u16 data:u8*
// Ways:       count:u64, for each: id:i64 ref_count:u32 refs:i64* tag_count:u8 [key:u16 val:u16]*
//
// Relations are not serialized — the renderer only uses nodes and ways.

namespace {

static inline void w8(std::FILE* f, uint8_t v)  { std::fwrite(&v, 1, 1, f); }
static inline void w16(std::FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }
static inline void w32(std::FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }
static inline void w64(std::FILE* f, uint64_t v) { std::fwrite(&v, 8, 1, f); }
static inline void wf64(std::FILE* f, double v)  { std::fwrite(&v, 8, 1, f); }
static inline void wi64(std::FILE* f, int64_t v)  { std::fwrite(&v, 8, 1, f); }

static inline uint8_t  r8(const uint8_t*& p)  { uint8_t v;  std::memcpy(&v, p, 1); p += 1; return v; }
static inline uint16_t r16(const uint8_t*& p) { uint16_t v; std::memcpy(&v, p, 2); p += 2; return v; }
static inline uint32_t r32(const uint8_t*& p) { uint32_t v; std::memcpy(&v, p, 4); p += 4; return v; }
static inline uint64_t r64(const uint8_t*& p) { uint64_t v; std::memcpy(&v, p, 8); p += 8; return v; }
static inline double   rf64(const uint8_t*& p) { double v;  std::memcpy(&v, p, 8); p += 8; return v; }
static inline int64_t  ri64(const uint8_t*& p) { int64_t v; std::memcpy(&v, p, 8); p += 8; return v; }

} // namespace

bool write_osm_binary(const OSMData& data, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    std::fwrite("TMR\0", 4, 1, f);
    w32(f, 1); // version

    // Build string table from way tags
    std::vector<std::string> strtab;
    std::unordered_map<std::string, uint16_t> str_to_idx;
    auto intern = [&](const std::string& s) -> uint16_t {
        auto it = str_to_idx.find(s);
        if (it != str_to_idx.end()) return it->second;
        uint16_t idx = static_cast<uint16_t>(strtab.size());
        strtab.push_back(s);
        str_to_idx[s] = idx;
        return idx;
    };

    for (const auto& [id, way] : data.ways)
        for (const auto& [k, v] : way.tags) { intern(k); intern(v); }

    // --- Nodes ---
    w64(f, data.nodes.size());
    for (const auto& [id, node] : data.nodes) {
        wi64(f, node.id);
        wf64(f, node.lat);
        wf64(f, node.lon);
    }

    // --- String table ---
    w32(f, static_cast<uint32_t>(strtab.size()));
    for (const auto& s : strtab) {
        w16(f, static_cast<uint16_t>(s.size()));
        std::fwrite(s.data(), 1, s.size(), f);
    }

    // --- Ways ---
    w64(f, data.ways.size());
    for (const auto& [id, way] : data.ways) {
        wi64(f, way.id);
        w32(f, static_cast<uint32_t>(way.node_refs.size()));
        for (auto ref : way.node_refs) wi64(f, ref);
        w8(f, static_cast<uint8_t>(way.tags.size()));
        for (const auto& [k, v] : way.tags) {
            w16(f, intern(k));
            w16(f, intern(v));
        }
    }

    std::fclose(f);
    return true;
}

OSMData read_osm_binary(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) throw std::runtime_error("Failed to open: " + path);

    struct stat st;
    fstat(fd, &st);
    size_t size = static_cast<size_t>(st.st_size);
    const uint8_t* map = static_cast<const uint8_t*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (map == MAP_FAILED) throw std::runtime_error("Failed to mmap: " + path);

    const uint8_t* p = map;

    if (size < 8 || std::memcmp(p, "TMR\0", 4) != 0) {
        munmap(const_cast<uint8_t*>(map), size);
        throw std::runtime_error("Invalid binary format: " + path);
    }
    p += 4;
    if (r32(p) != 1) {
        munmap(const_cast<uint8_t*>(map), size);
        throw std::runtime_error("Unsupported binary version: " + path);
    }

    OSMData data;

    // --- Nodes ---
    uint64_t node_count = r64(p);
    data.nodes.reserve(node_count);
    for (uint64_t i = 0; i < node_count; ++i) {
        int64_t id = ri64(p);
        double lat = rf64(p);
        double lon = rf64(p);
        data.nodes[id] = {id, lat, lon};
    }

    // --- String table ---
    uint32_t str_count = r32(p);
    std::vector<std::string> strtab;
    strtab.reserve(str_count);
    for (uint32_t i = 0; i < str_count; ++i) {
        uint16_t len = r16(p);
        strtab.emplace_back(reinterpret_cast<const char*>(p), len);
        p += len;
    }

    auto resolve = [&](uint16_t ki, uint16_t vi) -> std::pair<std::string, std::string> {
        return {ki < strtab.size() ? strtab[ki] : "", vi < strtab.size() ? strtab[vi] : ""};
    };

    // --- Ways ---
    uint64_t way_count = r64(p);
    data.ways.reserve(way_count);
    for (uint64_t i = 0; i < way_count; ++i) {
        int64_t id = ri64(p);
        uint32_t ref_count = r32(p);
        Way way;
        way.id = id;
        way.node_refs.reserve(ref_count);
        for (uint32_t j = 0; j < ref_count; ++j) way.node_refs.push_back(ri64(p));
        uint8_t tag_count = r8(p);
        for (uint8_t j = 0; j < tag_count; ++j) {
            uint16_t ki = r16(p);
            uint16_t vi = r16(p);
            auto [k, v] = resolve(ki, vi);
            way.tags[std::move(k)] = std::move(v);
        }
        data.ways[id] = std::move(way);
    }

    // Relations are not serialized — not used by renderer
    data.relations.reserve(0);

    munmap(const_cast<uint8_t*>(map), size);
    return data;
}
