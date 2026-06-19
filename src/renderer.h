#pragma once
#include "osm_model.h"
#include "image.h"
#include "style.h"
#include <utility>

struct Viewport {
    double center_lat;
    double center_lon;
    int zoom;
    int width;
    int height;
};

class Renderer {
public:
    Renderer(const OSMData& data);

    Image render(const Viewport& vp) const;

private:
    std::pair<int,int> project(double lat, double lon, const Viewport& vp) const;

    const OSMData& data_;
    StyleEngine style_;
};
