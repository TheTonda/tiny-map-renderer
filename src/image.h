#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Image {
public:
    int width, height;
    std::vector<uint32_t> pixels;

    Image(int w, int h);

    void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void set_pixel(int x, int y, uint32_t rgba);
    uint32_t get_pixel(int x, int y) const;

    // Unsafe: no bounds check — use only when coordinates are guaranteed in bounds
    void set_pixel_unsafe(int x, int y, uint32_t rgba);

    void fill(uint8_t r, uint8_t g, uint8_t b);
    bool write_ppm(const std::string& path) const;

    // Downsample a 2W×2H source image to W×H by averaging 2×2 blocks.
    // Source must have exactly 2× this image's dimensions.
    void downsample_2x(const Image& src);
};
