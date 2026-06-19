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

    raster::draw_line_thick(img, 10, 50, 90, 50, red, 5);
    check("thick horizontal: pixel within radius (50,52)", img.get_pixel(50, 52) == red);
    check("thick horizontal: pixel outside radius (50,56)", img.get_pixel(50, 56) != red);

    img.fill(255, 255, 255);
    uint32_t blue = pack(0, 0, 255);
    std::vector<std::pair<int,int>> poly = {{20, 20}, {20, 80}, {80, 80}};
    raster::draw_polyline(img, poly, blue, 3);
    check("polyline vertex (20,80) colored", img.get_pixel(20, 80) == blue);

    img.write_ppm("/tmp/test_thick.ppm");

    return failures > 0 ? 1 : 0;
}
