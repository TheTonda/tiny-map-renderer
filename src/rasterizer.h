#pragma once
#include <cstdint>
#include <utility>
#include <vector>

class Image;

namespace raster {

void draw_line(Image& img, int x0, int y0, int x1, int y1, uint32_t color);
void draw_line(Image& img, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
void draw_line_dashed(Image& img, int x0, int y0, int x1, int y1,
                      uint32_t color, int thickness, int dash_on, int dash_off);
void fill_polygon(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color);
void fill_disk(Image& img, int cx, int cy, int radius, uint32_t color);
void draw_line_thick(Image& img, int x0, int y0, int x1, int y1, uint32_t color, int thickness);
void draw_polyline(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color, int thickness);

}
