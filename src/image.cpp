#include "image.h"
#include <cstdio>
#include <cstdlib>
#include <string>

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

void Image::set_pixel_unsafe(int x, int y, uint32_t rgba) {
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

    // Write header
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);

    // Build pixel data in a buffer for a single fwrite call
    // (avoids 3 * width * height individual fwrite syscalls)
    size_t data_size = static_cast<size_t>(width) * height * 3;
    uint8_t* buf = static_cast<uint8_t*>(std::malloc(data_size));
    if (!buf) {
        std::fclose(f);
        return false;
    }

    size_t idx = 0;
    for (uint32_t p : pixels) {
        buf[idx++] = (p >> 24) & 0xFF;
        buf[idx++] = (p >> 16) & 0xFF;
        buf[idx++] = (p >> 8) & 0xFF;
    }

    std::fwrite(buf, 1, data_size, f);
    std::free(buf);
    std::fclose(f);
    return true;
}
