#include "image.h"
#include <cstdio>

Image::Image(int w, int h) : width(w), height(h), pixels(w * h, 0xFFFFFFFF) {}

void Image::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = (static_cast<uint32_t>(r) << 24)
                           | (static_cast<uint32_t>(g) << 16)
                           | (static_cast<uint32_t>(b) << 8)
                           | a;
}

void Image::set_pixel(int x, int y, uint32_t rgba) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = rgba;
}

uint32_t Image::get_pixel(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    return pixels[y * width + x];
}

void Image::fill(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t rgba = (static_cast<uint32_t>(r) << 24)
                  | (static_cast<uint32_t>(g) << 16)
                  | (static_cast<uint32_t>(b) << 8)
                  | 0xFF;
    for (auto& p : pixels) p = rgba;
}

bool Image::write_ppm(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (uint32_t p : pixels) {
        uint8_t r = (p >> 24) & 0xFF;
        uint8_t g = (p >> 16) & 0xFF;
        uint8_t b = (p >> 8) & 0xFF;
        std::fwrite(&r, 1, 1, f);
        std::fwrite(&g, 1, 1, f);
        std::fwrite(&b, 1, 1, f);
    }
    std::fclose(f);
    return true;
}
