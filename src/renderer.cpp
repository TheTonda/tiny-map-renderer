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
            const int margin = 1;
            const int cw = vp.width + margin;
            const int ch = vp.height + margin;

            auto inside = [&](int x, int y) { return x >= -margin && x < cw && y >= -margin && y < ch; };
            auto draw_seg = [&](int x0, int y0, int x1, int y1, uint32_t c, int w) {
                if (inside(x0, y0) && inside(x1, y1)) {
                    raster::draw_line_thick(img, x0, y0, x1, y1, c, w);
                } else if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, -margin, -margin, cw, ch)) {
                    raster::draw_line_thick(img, x0, y0, x1, y1, c, w);
                }
            };
            auto draw_seg_dashed = [&](int x0, int y0, int x1, int y1, uint32_t c, int w) {
                if (inside(x0, y0) && inside(x1, y1)) {
                    raster::draw_line_dashed(img, x0, y0, x1, y1, c, w, entry.style.dash_on, entry.style.dash_off);
                } else if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, -margin, -margin, cw, ch)) {
                    raster::draw_line_dashed(img, x0, y0, x1, y1, c, w, entry.style.dash_on, entry.style.dash_off);
                }
            };

            if (entry.style.dash_on > 0) {
                for (size_t i = 0; i + 1 < entry.pixels.size(); ++i)
                    draw_seg_dashed(entry.pixels[i].first, entry.pixels[i].second,
                                    entry.pixels[i+1].first, entry.pixels[i+1].second,
                                    entry.style.color, entry.style.width);
            } else {
                bool has_casing = entry.style.casing_width > 0;
                // Pass 1: all casing (same color, overlaps seamlessly)
                if (has_casing) {
                    int cw_total = entry.style.width + entry.style.casing_width;
                    for (size_t i = 0; i + 1 < entry.pixels.size(); ++i)
                        draw_seg(entry.pixels[i].first, entry.pixels[i].second,
                                 entry.pixels[i+1].first, entry.pixels[i+1].second,
                                 entry.style.casing_color, cw_total);
                }
                // Pass 2: all fill (on top, covers casing overlaps)
                for (size_t i = 0; i + 1 < entry.pixels.size(); ++i)
                    draw_seg(entry.pixels[i].first, entry.pixels[i].second,
                             entry.pixels[i+1].first, entry.pixels[i+1].second,
                             entry.style.color, entry.style.width);
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

            for (uint16_t j = 0; j < rw.refs_count; ++j) {
                uint32_t node_idx = rd.way_refs[rw.refs_offset + j];
                if (node_idx >= rd.nodes.size()) continue;

                // Cast to i64 before subtraction to avoid overflow in i32 subtraction
                double dx = static_cast<double>(static_cast<int64_t>(rd.nodes[node_idx].wx) - cx_ref) * scale;
                double dy = static_cast<double>(static_cast<int64_t>(rd.nodes[node_idx].wy) - cy_ref) * scale;
                int ix = static_cast<int>(dx + half_w);
                int iy = static_cast<int>(dy + half_h);
                pixels.push_back({ix, iy});
            }

            if (pixels.size() < 2) continue;

            // Rasterize — bbox check above already confirms overlap with tile.
            // Don't require any vertex to be inside the tile: ways can cross
            // tile boundaries with no vertex inside (common at max zoom).
            if (rw.style.fill) {
                raster::fill_polygon(img, pixels, rw.style.color);
            } else {
                // 1-pixel overlap clip margins: adjacent tiles draw boundary
                // pixels, eliminating gaps from ceil rounding at max zoom.
                const int margin = 1;
                const int cw = width + margin;
                const int ch = height + margin;

                auto inside = [=](int x, int y) { return x >= -margin && x < cw && y >= -margin && y < ch; };
                auto draw_seg = [&](int x0, int y0, int x1, int y1, uint32_t c, int w) {
                    if (inside(x0, y0) && inside(x1, y1)) {
                        raster::draw_line_thick(img, x0, y0, x1, y1, c, w);
                    } else if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, -margin, -margin, cw, ch)) {
                        raster::draw_line_thick(img, x0, y0, x1, y1, c, w);
                    }
                };
                auto draw_seg_dashed = [&](int x0, int y0, int x1, int y1, uint32_t c, int w) {
                    if (inside(x0, y0) && inside(x1, y1)) {
                        raster::draw_line_dashed(img, x0, y0, x1, y1, c, w, rw.style.dash_on, rw.style.dash_off);
                    } else if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, -margin, -margin, cw, ch)) {
                        raster::draw_line_dashed(img, x0, y0, x1, y1, c, w, rw.style.dash_on, rw.style.dash_off);
                    }
                };

                if (rw.style.dash_on > 0) {
                    for (size_t i = 0; i + 1 < pixels.size(); ++i)
                        draw_seg_dashed(pixels[i].first, pixels[i].second,
                                        pixels[i+1].first, pixels[i+1].second,
                                        rw.style.color, rw.style.width);
                } else {
                    bool has_casing = rw.style.casing_width > 0;
                    // Pass 1: all casing (same color, overlaps seamlessly)
                    if (has_casing) {
                        int cw_total = rw.style.width + rw.style.casing_width;
                        for (size_t i = 0; i + 1 < pixels.size(); ++i)
                            draw_seg(pixels[i].first, pixels[i].second,
                                     pixels[i+1].first, pixels[i+1].second,
                                     rw.style.casing_color, cw_total);
                    }
                    // Pass 2: all fill (covers casing overlaps — no visible seams)
                    for (size_t i = 0; i + 1 < pixels.size(); ++i)
                        draw_seg(pixels[i].first, pixels[i].second,
                                 pixels[i+1].first, pixels[i+1].second,
                                 rw.style.color, rw.style.width);
                }
            }
        }
    }

    return img;
}

// ============================================================
// Anti-aliased v2: render at 2× then downsample
// ============================================================

Image render_v2_aa(const RenderData& rd, double center_lat, double center_lon,
                   int zoom, int width, int height)
{
    Image hires = render_v2(rd, center_lat, center_lon, zoom + 1, width * 2, height * 2);
    Image result(width, height);
    result.downsample_2x(hires);
    return result;
}
