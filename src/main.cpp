#include "osm_parser.h"
#include "renderer.h"
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 8) {
        std::cerr << "Usage: " << argv[0] << " <osm-file> <lat> <lon> <zoom> <width> <height> <output.ppm>\n";
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
        std::cerr << "Error: invalid numeric argument: " << e.what() << "\n";
        return 1;
    }

    OSMData data;
    try {
        data = parse_osm_xml(osm_file);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "Parsed " << data.nodes.size() << " nodes, " << data.ways.size() << " ways\n";

    Viewport vp{lat, lon, zoom, width, height};

    Renderer renderer(data);
    std::cerr << "Rendering...\n";
    Image img = renderer.render(vp);

    if (!img.write_ppm(output_path)) {
        std::cerr << "Error: failed to write " << output_path << "\n";
        return 1;
    }

    std::cerr << "Wrote " << output_path << "\n";
    return 0;
}
