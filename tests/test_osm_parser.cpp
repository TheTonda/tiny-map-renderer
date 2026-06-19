#include "osm_parser.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static int failures = 0;

static void check(bool condition, const char* desc) {
    if (condition) {
        std::cout << "PASS: " << desc << '\n';
    } else {
        std::cout << "FAIL: " << desc << '\n';
        ++failures;
    }
}

int main() {
    auto data = parse_osm_xml("tests/test_data/minimal.osm");

    check(data.nodes.size() == 5, "parsed 5 nodes");
    check(data.ways.size() == 1, "parsed 1 way");
    check(data.relations.size() == 0, "parsed 0 relations");

    auto node_it = data.nodes.find(1);
    check(node_it != data.nodes.end(), "node 1 exists");
    check(std::abs(node_it->second.lat - 48.8550) < 1e-9 &&
          std::abs(node_it->second.lon - 2.2900) < 1e-9,
          "node 1 has correct lat/lon");

    auto way_it = data.ways.find(100);
    check(way_it != data.ways.end(), "way 100 exists");
    check(way_it->second.tags.at("highway") == "residential",
          "way has highway=residential tag");
    check(way_it->second.node_refs.size() == 5,
          "way has 5 node_refs");

    auto coords = data.get_way_coords(way_it->second);
    check(coords.size() == 5, "get_way_coords returns 5 pairs");

    auto bb = data.bounds();
    check(bb.min_lat >= 48.855 && bb.max_lat <= 48.860 &&
          bb.min_lon >= 2.290 && bb.max_lon <= 2.301,
          "bounds() returns reasonable values");

    bool threw = false;
    try {
        parse_osm_xml("tests/test_data/nonexistent.osm");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "non-existent file throws std::runtime_error");

    if (failures == 0) {
        std::cout << "\nAll tests passed.\n";
    } else {
        std::cout << '\n' << failures << " test(s) failed.\n";
    }
    return failures == 0 ? 0 : 1;
}
