#include "renderer.h"
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

    // Sort by z_order (background first)
    std::sort(visible_ways.begin(), visible_ways.end(),
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
