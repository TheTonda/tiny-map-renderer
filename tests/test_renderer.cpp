#include "renderer.h"
#include <cstdio>

int main() {
    OSMData data;

    data.nodes[1] = {1, 48.8584, 2.2945};
    data.nodes[2] = {2, 48.8585, 2.2950};
    data.nodes[3] = {3, 48.8586, 2.2955};
    data.nodes[4] = {4, 48.8587, 2.2960};
    data.nodes[5] = {5, 48.8588, 2.2965};

    Way road;
    road.id = 100;
    road.node_refs = {1, 2, 3, 4, 5};
    road.tags["highway"] = "primary";
    data.ways[100] = road;

    data.nodes[6] = {6, 48.8590, 2.2970};
    data.nodes[7] = {7, 48.8590, 2.2980};
    data.nodes[8] = {8, 48.8592, 2.2980};
    data.nodes[9] = {9, 48.8592, 2.2970};

    Way building;
    building.id = 200;
    building.node_refs = {6, 7, 8, 9};
    building.tags["building"] = "yes";
    data.ways[200] = building;

    Viewport vp{};
    vp.center_lat = 48.8588;
    vp.center_lon = 2.2960;
    vp.zoom = 16;
    vp.width = 512;
    vp.height = 512;

    Renderer renderer(data);
    Image img = renderer.render(vp);
    img.write_ppm("/tmp/test_render.ppm");

    const uint32_t bg_color = 0xF5F5F5FF;
    const uint32_t road_color = 0xF9B29CFF;

    int non_bg = 0;
    int road_pixels = 0;

    for (uint32_t p : img.pixels) {
        if (p != bg_color) {
            non_bg++;
            if (p == road_color) {
                road_pixels++;
            }
        }
    }

    bool check1 = non_bg >= 100;
    bool check2 = road_pixels > 0;

    std::printf("Check non-background pixels >= 100: %s (%d non-bg pixels)\n",
                check1 ? "PASS" : "FAIL", non_bg);
    std::printf("Check road-colored pixels > 0: %s (%d road pixels)\n",
                check2 ? "PASS" : "FAIL", road_pixels);

    return (check1 && check2) ? 0 : 1;
}
