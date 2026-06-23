#include "tile_format.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

bool write_tmr_tile(const Image& img, const std::string& path) {
    if (img.width <= 0 || img.height <= 0) return false;

    // Build palette: collect unique colors (RGB only, ignore alpha)
    std::unordered_map<uint32_t, uint8_t> color_to_idx; // RGB → palette index
    std::vector<uint8_t> palette; // sequential [r,g,b, r,g,b, ...]

    for (uint32_t p : img.pixels) {
        uint32_t rgb = p & 0xFFFFFF00; // mask off alpha
        if (color_to_idx.find(rgb) == color_to_idx.end()) {
            if (color_to_idx.size() >= 255) break; // max 255 + transparent
            color_to_idx[rgb] = static_cast<uint8_t>(palette.size() / 3);
            palette.push_back((p >> 24) & 0xFF);
            palette.push_back((p >> 16) & 0xFF);
            palette.push_back((p >> 8) & 0xFF);
        }
    }

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    std::fwrite("TILE", 4, 1, f);

    uint16_t w = static_cast<uint16_t>(img.width);
    uint16_t h = static_cast<uint16_t>(img.height);
    std::fwrite(&w, 2, 1, f);
    std::fwrite(&h, 2, 1, f);

    uint8_t psize = static_cast<uint8_t>(palette.size() / 3);
    if (psize == 0) psize = 1; // at least one color
    std::fwrite(&psize, 1, 1, f);
    std::fwrite(palette.data(), 1, palette.size(), f);

    // Write scanlines with RLE
    for (int y = 0; y < img.height; ++y) {
        // Collect runs for this scanline
        struct Run { uint16_t count; uint8_t idx; };
        std::vector<Run> runs;

        int run_start = 0;
        uint32_t prev_rgb = img.pixels[y * img.width] & 0xFFFFFF00;
        for (int x = 1; x < img.width; ++x) {
            uint32_t rgb = img.pixels[y * img.width + x] & 0xFFFFFF00;
            if (rgb != prev_rgb) {
                uint8_t idx = color_to_idx[prev_rgb];
                runs.push_back({static_cast<uint16_t>(x - run_start), idx});
                run_start = x;
                prev_rgb = rgb;
            }
        }
        // Last run
        uint8_t idx = color_to_idx[prev_rgb];
        runs.push_back({static_cast<uint16_t>(img.width - run_start), idx});

        uint16_t run_count = static_cast<uint16_t>(runs.size());
        std::fwrite(&run_count, 2, 1, f);
        for (const auto& r : runs) {
            std::fwrite(&r.count, 2, 1, f);
            std::fwrite(&r.idx, 1, 1, f);
        }
    }

    std::fclose(f);
    return true;
}
