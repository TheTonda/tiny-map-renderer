#include "image.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static int failures = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) {
        std::printf("PASS: %s\n", msg.c_str());
    } else {
        std::printf("FAIL: %s\n", msg.c_str());
        failures++;
    }
}

int main() {
    Image img(4, 4);

    img.fill(255, 0, 0);
    check(img.get_pixel(0, 0) == 0xFF0000FF, "fill red: pixel (0,0) is red");
    check(img.get_pixel(3, 3) == 0xFF0000FF, "fill red: pixel (3,3) is red");

    check(img.write_ppm("/tmp/test_image.ppm"), "write_ppm returns true");

    img.fill(0, 0, 0);

    img.set_pixel(1, 1, 10, 20, 30);
    check(img.get_pixel(1, 1) == 0x0A141EFF, "set_pixel rgba8 at (1,1)");

    img.set_pixel(2, 2, 0xAA5500FF);
    check(img.get_pixel(2, 2) == 0xAA5500FF, "set_pixel rgba32 at (2,2)");

    img.set_pixel(0, 0, 128, 255, 64, 128);
    check(img.get_pixel(0, 0) == 0x80FF4080, "set_pixel with alpha at (0,0)");

    check(img.get_pixel(0, 1) == 0x000000FF, "unaltered pixel stays black");

    check(img.get_pixel(-1, 0) == 0, "get_pixel OOB x=-1 returns 0");
    check(img.get_pixel(0, -1) == 0, "get_pixel OOB y=-1 returns 0");
    check(img.get_pixel(4, 0) == 0, "get_pixel OOB x=4 returns 0");
    check(img.get_pixel(0, 4) == 0, "get_pixel OOB y=4 returns 0");

    img.set_pixel(-1, 0, 0xFF00FFFF);
    img.set_pixel(0, -1, 0xFF00FFFF);
    img.set_pixel(4, 0, 0xFF00FFFF);
    img.set_pixel(0, 4, 0xFF00FFFF);
    check(true, "set_pixel OOB does not crash");

    if (img.write_ppm("/tmp/test_image.ppm")) {
        std::printf("PASS: re-write PPM succeeded\n");
    } else {
        std::printf("FAIL: re-write PPM failed\n");
        failures++;
    }

    std::printf("\n%d failures\n", failures);
    return failures ? 1 : 0;
}
