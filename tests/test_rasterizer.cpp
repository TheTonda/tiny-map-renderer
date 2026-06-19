#include "rasterizer.h"
#include "image.h"
#include <cstdio>
#include <cstdint>

static uint32_t pack(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r) << 24)
         | (static_cast<uint32_t>(g) << 16)
         | (static_cast<uint32_t>(b) << 8)
         | 0xFF;
}

static int failures = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        std::printf("PASS: %s\n", name);
    } else {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

int main() {
    const int W = 100, H = 100;
    Image img(W, H);
    img.fill(255, 255, 255);
    uint32_t red = pack(255, 0, 0);

    raster::draw_line(img, 10, 50, 90, 50, red);
    check("horizontal left endpoint", img.get_pixel(10, 50) == red);
    check("horizontal right endpoint", img.get_pixel(90, 50) == red);

    raster::draw_line(img, 50, 10, 50, 90, red);
    check("vertical top endpoint", img.get_pixel(50, 10) == red);
    check("vertical bottom endpoint", img.get_pixel(50, 90) == red);

    raster::draw_line(img, 10, 10, 90, 90, red);
    check("diagonal midpoint", img.get_pixel(50, 50) == red);

    raster::draw_line(img, 50, 10, 55, 90, red);
    check("steep line endpoint", img.get_pixel(55, 90) == red);

    img.fill(255, 255, 255);
    uint32_t blue = pack(0, 0, 255);
    raster::draw_line(img, 90, 30, 10, 30, blue);
    check("reverse left endpoint", img.get_pixel(10, 30) == blue);
    check("reverse right endpoint", img.get_pixel(90, 30) == blue);

    img.write_ppm("/tmp/test_lines.ppm");

    return failures > 0 ? 1 : 0;
}
