#include "rasterizer.h"
#include "image.h"
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

}
