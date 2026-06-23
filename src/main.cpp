#include "osm_parser.h"
#include "osm_binary.h"
#include "render_data.h"
#include "renderer.h"
#include "tile_format.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

static bool has_suffix(const std::string& s, const char* suffix) {
    size_t sl = std::strlen(suffix);
    return s.size() >= sl && s.compare(s.size() - sl, sl, suffix) == 0;
}

// Peek at file magic to detect format
static bool is_tmr_v2(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return false;
    char magic[4];
    bool ok = read(fd, magic, 4) == 4 && std::memcmp(magic, "TMR2", 4) == 0;
    close(fd);
    return ok;
}

int main(int argc, char* argv[]) {
    if (argc == 3 && std::strcmp(argv[1], "--compile") == 0) {
        // Compile mode: OSM XML → compact binary (v1)
        std::string in = argv[2];
        std::string out = in;
        if (has_suffix(out, ".osm")) out = out.substr(0, out.size() - 4);
        out += ".tmr";
        std::fprintf(stderr, "Compiling (v1) %s → %s\n", in.c_str(), out.c_str());

        OSMData data;
        try {
            data = parse_osm_xml(in);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error: %s\n", e.what());
            return 1;
        }
        std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", data.nodes.size(), data.ways.size());

        if (!write_osm_binary(data, out)) {
            std::fprintf(stderr, "Error: failed to write %s\n", out.c_str());
            return 1;
        }
        std::fprintf(stderr, "Wrote %s\n", out.c_str());
        return 0;
    }

    if (argc == 3 && std::strcmp(argv[1], "--compile-v2") == 0) {
        // Compile mode: OSM XML → pre-projected binary (v2)
        std::string in = argv[2];
        std::string out = in;
        if (has_suffix(out, ".osm")) out = out.substr(0, out.size() - 4);
        out += ".tmr";
        std::fprintf(stderr, "Compiling (v2, pre-projected) %s → %s\n", in.c_str(), out.c_str());

        OSMData data;
        try {
            data = parse_osm_xml(in);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error: %s\n", e.what());
            return 1;
        }
        std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", data.nodes.size(), data.ways.size());

        if (!write_osm_binary_v2(data, out)) {
            std::fprintf(stderr, "Error: failed to write %s\n", out.c_str());
            return 1;
        }
        std::fprintf(stderr, "Wrote %s\n", out.c_str());
        return 0;
    }

    if (argc != 8) {
        std::fprintf(stderr, "Usage: %s <file.osm|file.tmr> <lat> <lon> <zoom> <w> <h> <out.ppm>\n", argv[0]);
        std::fprintf(stderr, "       %s --compile <file.osm>\n", argv[0]);
        return 1;
    }

    std::string input_file = argv[1];
    double lat, lon;
    int zoom, width, height;
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
    RenderData rd;
    bool is_v2 = false;

    try {
        if (is_tmr_v2(input_file)) {
            rd = read_render_data(input_file);
            if (rd.nodes.empty()) {
                std::fprintf(stderr, "Error: failed to load v2 binary\n");
                return 1;
            }
            is_v2 = true;
            std::fprintf(stderr, "Loaded v2: %zu nodes, %zu ways, %d×%d grid\n",
                rd.nodes.size(), rd.ways.size(), rd.grid_cols, rd.grid_rows);
            // FIXME: v2 renderer not yet implemented, fall through to v1 path for now
        }
        if (!is_v2 && has_suffix(input_file, ".tmr")) {
            data = read_osm_binary(input_file);
        }
        if (!is_v2 && !has_suffix(input_file, ".tmr")) {
            data = parse_osm_xml(input_file);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    if (!is_v2) {
        std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", data.nodes.size(), data.ways.size());
    }

    Viewport vp{lat, lon, zoom, width, height};

    Image img(vp.width, vp.height);
    if (is_v2) {
        std::fprintf(stderr, "Rendering...\n");
        img = render_v2(rd, lat, lon, zoom, width, height);
    } else {
        Renderer renderer(data);
        std::fprintf(stderr, "Rendering...\n");
        img = renderer.render(vp);
    }

    bool ok;
    if (has_suffix(output_path, ".ttl") || has_suffix(output_path, ".tmr_tile")) {
        ok = write_tmr_tile(img, output_path);
    } else {
        ok = img.write_ppm(output_path);
    }
    if (!ok) {
        std::fprintf(stderr, "Error: failed to write %s\n", output_path.c_str());
        return 1;
    }

    std::fprintf(stderr, "Wrote %s\n", output_path.c_str());
    return 0;
}
