#include "osm_binary.h"
#include "render_data.h"
#include "style.h"
#include "tile_math.h"
#include <algorithm>
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

// ============================================================
// Helpers
// ============================================================

namespace {

static inline void w8(std::FILE* f, uint8_t v)  { std::fwrite(&v, 1, 1, f); }
static inline void w16(std::FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }
static inline void w32(std::FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }
static inline void w64(std::FILE* f, uint64_t v) { std::fwrite(&v, 8, 1, f); }
static inline void wf64(std::FILE* f, double v)  { std::fwrite(&v, 8, 1, f); }
static inline void wi32(std::FILE* f, int32_t v)  { std::fwrite(&v, 4, 1, f); }
static inline void wi64(std::FILE* f, int64_t v)  { std::fwrite(&v, 8, 1, f); }

static inline uint8_t  r8(const uint8_t*& p)  { uint8_t v;  std::memcpy(&v, p, 1); p += 1; return v; }
static inline uint16_t r16(const uint8_t*& p) { uint16_t v; std::memcpy(&v, p, 2); p += 2; return v; }
static inline uint32_t r32(const uint8_t*& p) { uint32_t v; std::memcpy(&v, p, 4); p += 4; return v; }
static inline uint64_t r64(const uint8_t*& p) { uint64_t v; std::memcpy(&v, p, 8); p += 8; return v; }
static inline double   rf64(const uint8_t*& p) { double v;  std::memcpy(&v, p, 8); p += 8; return v; }
static inline int32_t  ri32(const uint8_t*& p) { int32_t v; std::memcpy(&v, p, 4); p += 4; return v; }
static inline int64_t  ri64(const uint8_t*& p) { int64_t v; std::memcpy(&v, p, 8); p += 8; return v; }

} // namespace

// ============================================================
// v1 format (backward compatible, produces OSMData)
// ============================================================

bool write_osm_binary(const OSMData& data, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    std::fwrite("TMR\0", 4, 1, f);
    w32(f, 1); // version

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

    w64(f, data.nodes.size());
    for (const auto& [id, node] : data.nodes) {
        wi64(f, node.id);
        wf64(f, node.lat);
        wf64(f, node.lon);
    }

    w32(f, static_cast<uint32_t>(strtab.size()));
    for (const auto& s : strtab) {
        w16(f, static_cast<uint16_t>(s.size()));
        std::fwrite(s.data(), 1, s.size(), f);
    }

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

    uint64_t node_count = r64(p);
    data.nodes.reserve(node_count);
    for (uint64_t i = 0; i < node_count; ++i) {
        int64_t id = ri64(p);
        double lat = rf64(p);
        double lon = rf64(p);
        data.nodes[id] = {id, lat, lon};
    }

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

    munmap(const_cast<uint8_t*>(map), size);
    return data;
}

// ============================================================
// v2 format (pre-projected, grid-indexed, produces RenderData)
// ============================================================

bool write_osm_binary_v2(const OSMData& data, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // Compile step: pre-project, filter, build grid
    const int REF_ZOOM = 20;
    const int GRID_DIM = 64;

    StyleEngine style_engine;

    // First pass: collect styled ways and their node refs, compute bbox
    struct CompiledWay {
        const Way* way;
        Style style;
        std::vector<uint32_t> node_indices; // resolved indices into a flat node array
        int32_t min_wx, min_wy, max_wx, max_wy;
    };
    std::vector<CompiledWay> compiled_ways;

    // Build a flat node array: assign each node an index (0..N-1)
    // Also compute node IDs -> index mapping
    std::vector<RenderData::ProjNode> proj_nodes;
    proj_nodes.reserve(data.nodes.size());
    std::unordered_map<int64_t, uint32_t> node_id_to_idx;
    node_id_to_idx.reserve(data.nodes.size());

    for (const auto& [id, node] : data.nodes) {
        node_id_to_idx[id] = static_cast<uint32_t>(proj_nodes.size());
        int64_t wx = static_cast<int64_t>(TileMath::lon_to_world_px(node.lon, REF_ZOOM));
        int64_t wy = static_cast<int64_t>(TileMath::lat_to_world_py(node.lat, REF_ZOOM));
        proj_nodes.push_back({wx, wy});
    }

    // Compute world-pixel bounds of all nodes (for grid origin)
    double wx_min = static_cast<double>(proj_nodes[0].wx);
    double wy_min = static_cast<double>(proj_nodes[0].wy);
    double wx_max = wx_min, wy_max = wy_min;
    for (const auto& pn : proj_nodes) {
        double wx = static_cast<double>(pn.wx);
        double wy = static_cast<double>(pn.wy);
        if (wx < wx_min) wx_min = wx;
        if (wx > wx_max) wx_max = wx;
        if (wy < wy_min) wy_min = wy;
        if (wy > wy_max) wy_max = wy;
    }
    // Expand bounds by 1% for safety
    double pad_x = (wx_max - wx_min) * 0.01 + 1.0;
    double pad_y = (wy_max - wy_min) * 0.01 + 1.0;
    wx_min -= pad_x; wx_max += pad_x;
    wy_min -= pad_y; wy_max += pad_y;
    double cell_w = (wx_max - wx_min) / GRID_DIM;
    double cell_h = (wy_max - wy_min) / GRID_DIM;

    // Process ways: filter styled, resolve node refs to indices, compute bbox
    for (const auto& [id, way] : data.ways) {
        auto s = style_engine.style_for_way(way.tags);
        if (!s) continue;

        CompiledWay cw;
        cw.way = &way;
        cw.style = *s;
        cw.node_indices.reserve(way.node_refs.size());

        int64_t min_wx = INT64_MAX, min_wy = INT64_MAX;
        int64_t max_wx = INT64_MIN, max_wy = INT64_MIN;
        for (auto ref : way.node_refs) {
            auto it = node_id_to_idx.find(ref);
            if (it == node_id_to_idx.end()) continue;
            uint32_t idx = it->second;
            cw.node_indices.push_back(idx);
            int64_t wx = proj_nodes[idx].wx;
            int64_t wy = proj_nodes[idx].wy;
            if (wx < min_wx) min_wx = wx;
            if (wx > max_wx) max_wx = wx;
            if (wy < min_wy) min_wy = wy;
            if (wy > max_wy) max_wy = wy;
        }
        if (cw.node_indices.size() < 2) continue;

        cw.min_wx = static_cast<int32_t>(min_wx);
        cw.min_wy = static_cast<int32_t>(min_wy);
        cw.max_wx = static_cast<int32_t>(max_wx);
        cw.max_wy = static_cast<int32_t>(max_wy);
        compiled_ways.push_back(std::move(cw));
    }

    // Build spatial grid
    struct CellBucket {
        std::vector<uint32_t> way_indices;
    };
    std::vector<CellBucket> grid_buckets(GRID_DIM * GRID_DIM);

    for (uint32_t wi = 0; wi < compiled_ways.size(); ++wi) {
        const auto& cw = compiled_ways[wi];
        int cx0 = static_cast<int>((cw.min_wx - wx_min) / cell_w);
        int cy0 = static_cast<int>((cw.min_wy - wy_min) / cell_h);
        int cx1 = static_cast<int>((cw.max_wx - wx_min) / cell_w);
        int cy1 = static_cast<int>((cw.max_wy - wy_min) / cell_h);
        if (cx0 < 0) cx0 = 0;
        if (cy0 < 0) cy0 = 0;
        if (cx1 >= GRID_DIM) cx1 = GRID_DIM - 1;
        if (cy1 >= GRID_DIM) cy1 = GRID_DIM - 1;

        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                grid_buckets[cy * GRID_DIM + cx].way_indices.push_back(wi);
            }
        }
    }

    // Build style palette
    std::vector<Style> style_palette;
    std::vector<uint8_t> way_style_idx(compiled_ways.size());

    for (uint32_t wi = 0; wi < compiled_ways.size(); ++wi) {
        const auto& s = compiled_ways[wi].style;
        uint8_t idx = 0;
        for (; idx < style_palette.size(); ++idx) {
            if (style_palette[idx].color == s.color &&
                style_palette[idx].width == s.width &&
                style_palette[idx].fill == s.fill &&
                style_palette[idx].z_order == s.z_order) break;
        }
        if (idx == style_palette.size()) style_palette.push_back(s);
        way_style_idx[wi] = idx;
    }

    // --- Write v2 binary ---
    std::fwrite("TMR2", 4, 1, f);
    w32(f, 2); // version

    // Header
    w32(f, REF_ZOOM);
    w32(f, GRID_DIM);  // grid_cols
    w32(f, GRID_DIM);  // grid_rows
    wf64(f, wx_min);
    wf64(f, wy_min);
    wf64(f, cell_w);
    wf64(f, cell_h);

    // Node section
    w64(f, proj_nodes.size());
    for (const auto& pn : proj_nodes) {
        wi64(f, pn.wx);
        wi64(f, pn.wy);
    }

    // Style palette
    uint8_t psize = static_cast<uint8_t>(style_palette.size());
    w8(f, psize);
    for (const auto& s : style_palette) {
        w32(f, s.color);
        w8(f, static_cast<uint8_t>(s.width));
        w8(f, s.fill ? 1 : 0);
        w8(f, static_cast<uint8_t>(s.z_order));
    }

    // Way section
    w64(f, compiled_ways.size());
    uint32_t refs_total = 0;
    for (const auto& cw : compiled_ways) {
        wi32(f, cw.min_wx);
        wi32(f, cw.min_wy);
        wi32(f, cw.max_wx);
        wi32(f, cw.max_wy);
        w32(f, refs_total); // refs_offset
        w16(f, static_cast<uint16_t>(cw.node_indices.size()));
        w8(f, way_style_idx[&cw - compiled_ways.data()]);
        // z_order not stored separately — derived from style
        refs_total += static_cast<uint32_t>(cw.node_indices.size());
    }

    // Node refs section
    w64(f, refs_total);
    for (const auto& cw : compiled_ways) {
        for (uint32_t idx : cw.node_indices) {
            w32(f, idx);
        }
    }

    // Grid section
    uint64_t total_grid_ways = 0;
    for (const auto& cell : grid_buckets) {
        total_grid_ways += cell.way_indices.size();
    }

    for (const auto& cell : grid_buckets) {
        w16(f, static_cast<uint16_t>(cell.way_indices.size()));
    }
    // (offsets computed implicitly: sequential)

    w64(f, total_grid_ways);
    // Grid way indices (offsets are sequential, not stored explicitly)
    // The offset for cell i is the sum of counts for cells 0..i-1
    uint32_t running_offset = 0;
    for (const auto& cell : grid_buckets) {
        (void)running_offset; // offsets are implicit from sequential write
        for (uint32_t wi : cell.way_indices) {
            w32(f, wi);
        }
    }

    std::fclose(f);
    return true;
}

// ============================================================
// v2 reader: loads into RenderData
// ============================================================

RenderData read_render_data(const std::string& path) {
    RenderData rd;

    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return rd;

    struct stat st;
    fstat(fd, &st);
    size_t size = static_cast<size_t>(st.st_size);
    const uint8_t* map = static_cast<const uint8_t*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (map == MAP_FAILED) return rd;

    const uint8_t* p = map;

    if (size < 12 || std::memcmp(p, "TMR2", 4) != 0) {
        munmap(const_cast<uint8_t*>(map), size);
        return rd;
    }
    p += 4;
    if (r32(p) != 2) {
        munmap(const_cast<uint8_t*>(map), size);
        return rd;
    }

    // Header
    rd.reference_zoom = static_cast<int>(r32(p));
    rd.grid_cols = static_cast<int>(r32(p));
    rd.grid_rows = static_cast<int>(r32(p));
    rd.grid_origin_wx = rf64(p);
    rd.grid_origin_wy = rf64(p);
    rd.grid_cell_w = rf64(p);
    rd.grid_cell_h = rf64(p);

    // Nodes
    uint64_t node_count = r64(p);
    rd.nodes.resize(node_count);
    for (uint64_t i = 0; i < node_count; ++i) {
        rd.nodes[i].wx = ri64(p);
        rd.nodes[i].wy = ri64(p);
    }

    // Style palette
    uint8_t style_count = r8(p);
    std::vector<Style> styles(style_count);
    for (uint8_t i = 0; i < style_count; ++i) {
        styles[i].color = r32(p);
        styles[i].width = r8(p);
        styles[i].fill = r8(p) != 0;
        styles[i].z_order = r8(p);
    }

    // Ways
    uint64_t way_count = r64(p);
    rd.ways.resize(way_count);
    for (uint64_t i = 0; i < way_count; ++i) {
        auto& rw = rd.ways[i];
        rw.min_wx = ri32(p);
        rw.min_wy = ri32(p);
        rw.max_wx = ri32(p);
        rw.max_wy = ri32(p);
        rw.refs_offset = r32(p);
        rw.refs_count = r16(p);
        uint8_t sidx = r8(p);
        if (sidx < style_count) {
            rw.style = styles[sidx];
            rw.z_order = rw.style.z_order;
        }
    }

    // Node refs
    uint64_t total_refs = r64(p);
    rd.way_refs.resize(total_refs);
    for (uint64_t i = 0; i < total_refs; ++i) {
        rd.way_refs[i] = r32(p);
    }

    // Grid: read cell counts, compute offsets sequentially
    rd.grid.resize(rd.grid_cols * rd.grid_rows);
    uint32_t offset = 0;
    for (int i = 0; i < rd.grid_cols * rd.grid_rows; ++i) {
        rd.grid[i].count = r16(p);
        rd.grid[i].offset = offset;
        offset += rd.grid[i].count;
    }

    // Grid way indices
    uint64_t total_grid_ways = r64(p);
    rd.grid_ways.resize(total_grid_ways);
    for (uint64_t i = 0; i < total_grid_ways; ++i) {
        rd.grid_ways[i] = r32(p);
    }

    munmap(const_cast<uint8_t*>(map), size);
    return rd;
}
