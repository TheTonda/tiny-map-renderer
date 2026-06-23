#include "renderer.h"
#include "render_data.h"
#include "tile_math.h"
#include "rasterizer.h"
#include "clip.h"
#include <algorithm>
#include <vector>

Renderer::Renderer(const OSMData& data) : data_(data) {}

std::pair<int,int> Renderer::project(double lat, double lon, const Viewport& vp) const {
    double wx = TileMath::lon_to_world_px(lon, vp.zoom);
    double wy = TileMath::lat_to_world_py(lat, vp.zoom);
    double cx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom);
    double cy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom);
    int ix = static_cast<int>(wx - cx + vp.width / 2.0);
    int iy = static_cast<int>(wy - cy + vp.height / 2.0);
    return {ix, iy};
}

Image Renderer::render(const Viewport& vp) const {
    Image img(vp.width, vp.height);
    img.fill(0xF5, 0xF5, 0xF5);

    // Pre-compute center in world-pixel space (reused for all vertex projections)
    double cx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom);
    double cy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom);
    double half_w = vp.width / 2.0;
    double half_h = vp.height / 2.0;

    struct WayEntry {
        std::vector<std::pair<int,int>> pixels;
        Style style;
        int z_order;
    };
    std::vector<WayEntry> visible_ways;

    for (const auto& [id, way] : data_.ways) {
        auto style_opt = style_.style_for_way(way.tags);
        if (!style_opt) continue;

        // Single pass: project nodes AND check visibility in one traversal
        std::vector<std::pair<int,int>> pixels;
        pixels.reserve(way.node_refs.size());

        bool any_visible = false;
        for (auto ref : way.node_refs) {
            auto it = data_.nodes.find(ref);
            if (it == data_.nodes.end()) continue;
            const Node& node = it->second;

            double wx = TileMath::lon_to_world_px(node.lon, vp.zoom);
            double wy = TileMath::lat_to_world_py(node.lat, vp.zoom);
            int ix = static_cast<int>(wx - cx + half_w);
            int iy = static_cast<int>(wy - cy + half_h);
            pixels.push_back({ix, iy});

            any_visible |= (ix >= 0 && ix < vp.width && iy >= 0 && iy < vp.height);
        }

        if (!any_visible || pixels.size() < 2) continue;

        visible_ways.push_back({std::move(pixels), *style_opt, style_opt->z_order});
    }

    // Sort by z_order (background first). Use stable sort so
    // that same-z_order ways render consistently regardless of
    // hash map iteration order (important for binary vs XML parity).
    std::stable_sort(visible_ways.begin(), visible_ways.end(),
        [](const WayEntry& a, const WayEntry& b) {
            return a.z_order < b.z_order;
        });

    // Rasterize — coordinates already projected, no double lookups
    for (const auto& entry : visible_ways) {
        if (entry.style.fill) {
            raster::fill_polygon(img, entry.pixels, entry.style.color);
        } else {
            for (size_t i = 0; i + 1 < entry.pixels.size(); ++i) {
                int x0 = entry.pixels[i].first;
                int y0 = entry.pixels[i].second;
                int x1 = entry.pixels[i + 1].first;
                int y1 = entry.pixels[i + 1].second;
                if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, 0, 0, vp.width, vp.height)) {
                    raster::draw_line_thick(img, x0, y0, x1, y1, entry.style.color, entry.style.width);
                }
            }
        }
    }

    return img;
}

// ============================================================
// v2 renderer: pre-projected data, grid filtering, zero trig
// ============================================================

Image render_v2(const RenderData& rd, double center_lat, double center_lon,
                int zoom, int width, int height)
{
    Image img(width, height);
    img.fill(0xF5, 0xF5, 0xF5);

    if (rd.nodes.empty() || rd.ways.empty()) return img;

    int ref_zoom = rd.reference_zoom;
    int shift = ref_zoom - zoom;  // right-shift world-pixels from ref_zoom to target zoom
    if (shift < 0) shift = 0;     // don't upscale beyond ref_zoom

    // Compute viewport center in world-pixel at reference zoom
    int64_t cx_ref = static_cast<int64_t>(TileMath::lon_to_world_px(center_lon, ref_zoom));
    int64_t cy_ref = static_cast<int64_t>(TileMath::lat_to_world_py(center_lat, ref_zoom));

    // Half viewport in target-zoom pixels, then up-shifted to reference zoom
    int64_t half_w_ref = static_cast<int64_t>(width / 2) << shift;
    int64_t half_h_ref = static_cast<int64_t>(height / 2) << shift;

    // Viewport bounds in world-pixel at reference zoom
    double vp_wx_min = static_cast<double>(cx_ref - half_w_ref);
    double vp_wy_min = static_cast<double>(cy_ref - half_h_ref);
    double vp_wx_max = static_cast<double>(cx_ref + half_w_ref);
    double vp_wy_max = static_cast<double>(cy_ref + half_h_ref);

    // Scale factor: 1 pixel at target zoom = (1 << shift) world-pixels at ref_zoom
    double scale = 1.0 / static_cast<double>(1LL << shift);
    double half_w = width / 2.0;
    double half_h = height / 2.0;

    // Query spatial grid for visible ways
    std::vector<uint32_t> way_indices;
    rd.query_grid(vp_wx_min, vp_wy_min, vp_wx_max, vp_wy_max, way_indices);
    if (way_indices.empty()) return img;

    // Bucket ways by z_order to avoid full sort
    constexpr int MAX_Z = 32;
    std::vector<uint32_t> z_buckets[MAX_Z];
    for (uint32_t wi : way_indices) {
        if (wi < rd.ways.size()) {
            int z = rd.ways[wi].z_order;
            if (z < 0) z = 0;
            if (z >= MAX_Z) z = MAX_Z - 1;
            z_buckets[z].push_back(wi);
        }
    }

    for (int z = 0; z < MAX_Z; ++z) {
        for (uint32_t wi : z_buckets[z]) {
            const auto& rw = rd.ways[wi];

            // Fast bbox check: skip if way bbox doesn't overlap viewport
            if (rw.max_wx < cx_ref - half_w_ref || rw.min_wx > cx_ref + half_w_ref ||
                rw.max_wy < cy_ref - half_h_ref || rw.min_wy > cy_ref + half_h_ref) continue;

            // Project nodes: subtract center first, then scale (preserves precision)
            std::vector<std::pair<int,int>> pixels;
            pixels.reserve(rw.refs_count);

            bool any_visible = false;
            for (uint16_t j = 0; j < rw.refs_count; ++j) {
                uint32_t node_idx = rd.way_refs[rw.refs_offset + j];
                if (node_idx >= rd.nodes.size()) continue;

                // Cast to i64 before subtraction to avoid overflow in i32 subtraction
                double dx = static_cast<double>(static_cast<int64_t>(rd.nodes[node_idx].wx) - cx_ref) * scale;
                double dy = static_cast<double>(static_cast<int64_t>(rd.nodes[node_idx].wy) - cy_ref) * scale;
                int ix = static_cast<int>(dx + half_w);
                int iy = static_cast<int>(dy + half_h);
                pixels.push_back({ix, iy});

                any_visible |= (ix >= 0 && ix < width && iy >= 0 && iy < height);
            }

            if (!any_visible || pixels.size() < 2) continue;

            // Rasterize
            if (rw.style.fill) {
                raster::fill_polygon(img, pixels, rw.style.color);
            } else {
                for (size_t i = 0; i + 1 < pixels.size(); ++i) {
                    int x0 = pixels[i].first;
                    int y0 = pixels[i].second;
                    int x1 = pixels[i + 1].first;
                    int y1 = pixels[i + 1].second;
                    if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, 0, 0, width, height)) {
                        raster::draw_line_thick(img, x0, y0, x1, y1, rw.style.color, rw.style.width);
                    }
                }
            }
        }
    }

    return img;
}
