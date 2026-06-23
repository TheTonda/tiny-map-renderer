#include "osm_parser.h"
#include "renderer.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 8) {
        std::fprintf(stderr, "Usage: %s <osm-file> <lat> <lon> <zoom> <width> <height> <output.ppm>\n", argv[0]);
        return 1;
    }

    std::string osm_file = argv[1];
    double lat;
    double lon;
    int zoom;
    int width;
    int height;
    std::string output_path = argv[7];

    try {
        lat = std::stod(argv[2]);
        lon = std::stod(argv[3]);
        zoom = std::stoi(argv[4]);
        width = std::stoi(argv[5]);
        height = std::stoi(argv[6]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: invalid numeric argument: %s\n", e.what());
        return 1;
    }

    OSMData data;
    try {
        data = parse_osm_xml(osm_file);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", data.nodes.size(), data.ways.size());

    Viewport vp{lat, lon, zoom, width, height};

    Renderer renderer(data);
    std::fprintf(stderr, "Rendering...\n");
    Image img = renderer.render(vp);

    if (!img.write_ppm(output_path)) {
        std::fprintf(stderr, "Error: failed to write %s\n", output_path.c_str());
        return 1;
    }

    std::fprintf(stderr, "Wrote %s\n", output_path.c_str());
    return 0;
}
