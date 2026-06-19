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

    double cx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom);
    double cy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom);
    double top_left_px = cx - vp.width / 2.0;
    double top_left_py = cy - vp.height / 2.0;
    double bot_right_px = cx + vp.width / 2.0;
    double bot_right_py = cy + vp.height / 2.0;
    double north = TileMath::world_py_to_lat(top_left_py, vp.zoom);
    double south = TileMath::world_py_to_lat(bot_right_py, vp.zoom);
    double west = TileMath::world_px_to_lon(top_left_px, vp.zoom);
    double east = TileMath::world_px_to_lon(bot_right_px, vp.zoom);

    struct WayEntry {
        const Way* way;
        Style style;
    };
    std::vector<WayEntry> visible_ways;

    for (const auto& [id, way] : data_.ways) {
        auto style_opt = style_.style_for_way(way.tags);
        if (!style_opt) continue;

        bool visible = false;
        for (auto ref : way.node_refs) {
            auto it = data_.nodes.find(ref);
            if (it != data_.nodes.end()) {
                const Node& node = it->second;
                if (node.lat >= south && node.lat <= north &&
                    node.lon >= west && node.lon <= east) {
                    visible = true;
                    break;
                }
            }
        }
        if (!visible) continue;

        visible_ways.push_back({&way, *style_opt});
    }

    std::sort(visible_ways.begin(), visible_ways.end(),
        [](const WayEntry& a, const WayEntry& b) {
            return a.style.z_order < b.style.z_order;
        });

    for (const auto& entry : visible_ways) {
        auto coords = data_.get_way_coords(*entry.way);
        if (coords.size() < 2) continue;

        std::vector<std::pair<int,int>> pixels;
        pixels.reserve(coords.size());
        for (const auto& [lat, lon] : coords) {
            pixels.push_back(project(lat, lon, vp));
        }

        if (entry.style.fill) {
            bool any_inside = false;
            for (const auto& p : pixels) {
                if (p.first >= 0 && p.first < vp.width &&
                    p.second >= 0 && p.second < vp.height) {
                    any_inside = true;
                    break;
                }
            }
            if (any_inside) {
                raster::fill_polygon(img, pixels, entry.style.color);
            }
        } else {
            for (size_t i = 0; i + 1 < pixels.size(); ++i) {
                int x0 = pixels[i].first;
                int y0 = pixels[i].second;
                int x1 = pixels[i + 1].first;
                int y1 = pixels[i + 1].second;
                if (clip_line_cohen_sutherland_int(x0, y0, x1, y1, 0, 0, vp.width, vp.height)) {
                    raster::draw_line_thick(img, x0, y0, x1, y1, entry.style.color, entry.style.width);
                }
            }
        }
    }

    return img;
}
