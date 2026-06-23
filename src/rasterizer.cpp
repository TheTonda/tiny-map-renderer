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
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void draw_line(Image& img, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = (static_cast<uint32_t>(r) << 24)
                   | (static_cast<uint32_t>(g) << 16)
                   | (static_cast<uint32_t>(b) << 8)
                   | 0xFF;
    draw_line(img, x0, y0, x1, y1, color);
}

void fill_disk(Image& img, int cx, int cy, int radius, uint32_t color) {
    int r2 = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r2) img.set_pixel(x, y, color);
        }
    }
}

void draw_line_thick(Image& img, int x0, int y0, int x1, int y1, uint32_t color, int thickness) {
    if (thickness <= 1) {
        draw_line(img, x0, y0, x1, y1, color);
        return;
    }

    // Use polygon fill for thick lines — much faster than per-step fill_disk.
    // Compute the 4 corners of the thick line segment rectangle.
    int dx = x1 - x0;
    int dy = y1 - y0;
    double len = std::sqrt(static_cast<double>(dx)*dx + static_cast<double>(dy)*dy);
    if (len < 0.5) {
        fill_disk(img, x0, y0, thickness / 2, color);
        return;
    }

    double half = thickness * 0.5;
    double nx = -dy * half / len;
    double ny =  dx * half / len;

    // Use ceil for all corners to ensure the quad fully covers the line
    // (truncation would miss pixels at the outer edges)
    std::vector<std::pair<int,int>> quad = {
        {static_cast<int>(std::ceil(x0 + nx)), static_cast<int>(std::ceil(y0 + ny))},
        {static_cast<int>(std::ceil(x1 + nx)), static_cast<int>(std::ceil(y1 + ny))},
        {static_cast<int>(std::ceil(x1 - nx)), static_cast<int>(std::ceil(y1 - ny))},
        {static_cast<int>(std::ceil(x0 - nx)), static_cast<int>(std::ceil(y0 - ny))}
    };

    fill_polygon(img, quad, color);
}

void draw_polyline(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color, int thickness) {
    if (points.empty()) return;
    int radius = thickness / 2;

    if (points.size() == 1) {
        fill_disk(img, points[0].first, points[0].second, radius, color);
        return;
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        draw_line_thick(img, points[i].first, points[i].second,
                        points[i + 1].first, points[i + 1].second, color, thickness);
    }

    // Round caps at vertices
    for (size_t i = 0; i < points.size(); ++i) {
        fill_disk(img, points[i].first, points[i].second, radius, color);
    }
}

void fill_polygon(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color) {
    const size_t n = points.size();
    if (n < 3) return;

    int min_y = points[0].second;
    int max_y = points[0].second;
    for (size_t i = 1; i < n; ++i) {
        int y = points[i].second;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }

    // Clamp to image bounds
    if (min_y < 0) min_y = 0;
    if (max_y >= img.height) max_y = img.height - 1;
    if (min_y > max_y) return;

    // Pre-allocate intersection vector once
    std::vector<int> intersections;
    intersections.reserve(n);

    for (int y = min_y; y <= max_y; ++y) {
        intersections.clear();

        for (size_t i = 0; i < n; ++i) {
            int y0 = points[i].second;
            int y1 = points[(i + 1) % n].second;
            if (y0 == y1) continue;

            int edge_min = y0 < y1 ? y0 : y1;
            int edge_max = y0 > y1 ? y0 : y1;
            if (y < edge_min || y >= edge_max) continue;

            int x0 = points[i].first;
            int x1 = points[(i + 1) % n].first;

            // Integer fixed-point: multiply before divide for precision
            int64_t num = static_cast<int64_t>(x1 - x0) * (y - y0);
            int denom = y1 - y0;
            int x = static_cast<int>(x0 + num / denom);

            // Ceil rounding: increment when fractional part exists
            if (num % denom != 0 && (denom > 0) == (num > 0)) ++x;

            intersections.push_back(x);
        }

        // Inline sort for small N (common case: 2-8 intersections)
        if (intersections.size() <= 1) continue;
        if (intersections.size() == 2) {
            if (intersections[0] > intersections[1])
                std::swap(intersections[0], intersections[1]);
        } else {
            std::sort(intersections.begin(), intersections.end());
        }

        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x_start = intersections[i];
            int x_end   = intersections[i + 1];
            if (x_start < 0) x_start = 0;
            if (x_end > img.width) x_end = img.width;
            for (int x = x_start; x < x_end; ++x) {
                img.set_pixel_unsafe(x, y, color);
            }
        }
    }
}

} // namespace raster
