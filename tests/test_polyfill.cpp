#include "image.h"
#include "rasterizer.h"
#include <cstdio>
#include <vector>

static int failures = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("PASS: %s\n", msg);
    } else {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

int main() {
    const int W = 100, H = 100;

    {
        Image img(W, H);
        uint32_t red = 0xFF0000FF;
        std::vector<std::pair<int,int>> square = {{10,10},{90,10},{90,90},{10,90}};
        raster::fill_polygon(img, square, red);
        check(img.get_pixel(50, 50) == red, "square: center (50,50) filled");
        check(img.get_pixel(5, 5) != red, "square: outside (5,5) not filled");
    }

    {
        Image img(W, H);
        uint32_t red = 0xFF0000FF;
        std::vector<std::pair<int,int>> tri = {{50,10},{10,90},{90,90}};
        raster::fill_polygon(img, tri, red);
        check(img.get_pixel(50, 60) == red, "triangle: (50,60) filled");
        check(img.get_pixel(50, 5) != red, "triangle: (50,5) not filled");
    }

    {
        Image img(W, H);
        uint32_t red = 0xFF0000FF;
        std::vector<std::pair<int,int>> cross = {
            {30,10},{70,10},{70,30},{90,30},{90,70},{70,70},
            {70,90},{30,90},{30,70},{10,70},{10,30},{30,30}
        };
        raster::fill_polygon(img, cross, red);
        check(img.get_pixel(50, 50) == red, "cross: center (50,50) filled");
        check(img.get_pixel(5, 5) != red, "cross: far corner (5,5) not filled");
    }

    {
        Image img(W, H);
        std::vector<std::pair<int,int>> square = {{10,10},{90,10},{90,90},{10,90}};
        std::vector<std::pair<int,int>> tri = {{50,10},{10,90},{90,90}};
        raster::fill_polygon(img, square, 0x00FF00FF);
        raster::fill_polygon(img, tri, 0xFF0000FF);
        img.write_ppm("/tmp/test_polyfill.ppm");
    }

    std::printf("%s\n", failures ? "SOME TESTS FAILED" : "All tests passed");
    return failures ? 1 : 0;
}
