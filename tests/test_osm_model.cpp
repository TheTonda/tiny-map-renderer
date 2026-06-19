#include "osm_model.h"
#include <cstdlib>
#include <iostream>

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
    OSMData data;

    data.nodes[1] = {1, 50.0, 14.0};
    data.nodes[2] = {2, 50.1, 14.1};
    data.nodes[3] = {3, 50.2, 14.2};
    data.nodes[4] = {4, 50.3, 14.3};
    data.nodes[5] = {5, 50.4, 14.4};

    Way way;
    way.id = 100;
    way.node_refs = {1, 2, 3, 4, 5};
    way.tags["highway"] = "residential";

    auto coords = data.get_way_coords(way);
    check(coords.size() == 5, "get_way_coords returns 5 coordinate pairs");

    check(coords[0].first == 50.0 && coords[0].second == 14.0, "coord 0 matches node 1");
    check(coords[1].first == 50.1 && coords[1].second == 14.1, "coord 1 matches node 2");
    check(coords[2].first == 50.2 && coords[2].second == 14.2, "coord 2 matches node 3");
    check(coords[3].first == 50.3 && coords[3].second == 14.3, "coord 3 matches node 4");
    check(coords[4].first == 50.4 && coords[4].second == 14.4, "coord 4 matches node 5");

    check(way.tags.at("highway") == "residential", "way tags contain highway=residential");

    auto bb = data.bounds();
    check(bb.min_lat == 50.0, "bounds min_lat");
    check(bb.max_lat == 50.4, "bounds max_lat");
    check(bb.min_lon == 14.0, "bounds min_lon");
    check(bb.max_lon == 14.4, "bounds max_lon");

    OSMData empty;
    auto bb_empty = empty.bounds();
    check(bb_empty.min_lat == 0 && bb_empty.max_lat == 0 &&
          bb_empty.min_lon == 0 && bb_empty.max_lon == 0,
          "empty OSMData bounds returns {0,0,0,0}");

    auto missing_coords = empty.get_way_coords(way);
    check(missing_coords.empty(), "get_way_coords with missing refs returns empty");

    if (failures == 0) {
        std::cout << "\nAll tests passed.\n";
    } else {
        std::cout << '\n' << failures << " test(s) failed.\n";
    }
    return failures == 0 ? 0 : 1;
}
