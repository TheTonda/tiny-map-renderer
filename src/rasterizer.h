#pragma once
#include <cstdint>
#include <utility>
#include <vector>

class Image;

namespace raster {

void draw_line(Image& img, int x0, int y0, int x1, int y1, uint32_t color);
void draw_line(Image& img, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
void fill_polygon(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color);

}
