#include "rasterizer.h"
#include "image.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace raster {

void draw_line(Image& img, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        img.set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_line(Image& img, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = (static_cast<uint32_t>(r) << 24)
                   | (static_cast<uint32_t>(g) << 16)
                   | (static_cast<uint32_t>(b) << 8)
                   | 0xFF;
    draw_line(img, x0, y0, x1, y1, color);
}

void fill_polygon(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color) {
    if (points.size() < 3) return;

    int min_y = points[0].second;
    int max_y = points[0].second;
    for (const auto& p : points) {
        if (p.second < min_y) min_y = p.second;
        if (p.second > max_y) max_y = p.second;
    }

    const size_t n = points.size();

    for (int y = min_y; y <= max_y; ++y) {
        std::vector<float> intersections;

        for (size_t i = 0; i < n; ++i) {
            int x0 = points[i].first;
            int y0 = points[i].second;
            int x1 = points[(i + 1) % n].first;
            int y1 = points[(i + 1) % n].second;

            int edge_min_y = y0 < y1 ? y0 : y1;
            int edge_max_y = y0 > y1 ? y0 : y1;

            if (y >= edge_min_y && y < edge_max_y) {
                float x = static_cast<float>(x0)
                    + (static_cast<float>(y - y0) * static_cast<float>(x1 - x0))
                    / static_cast<float>(y1 - y0);
                intersections.push_back(x);
            }
        }

        std::sort(intersections.begin(), intersections.end());

        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x_start = static_cast<int>(std::ceil(intersections[i]));
            int x_end   = static_cast<int>(std::ceil(intersections[i + 1]));
            for (int x = x_start; x < x_end; ++x) {
                img.set_pixel(x, y, color);
            }
        }
    }
}

}
