#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Node {
    int64_t id;
    double lat;
    double lon;
};

struct Way {
    int64_t id;
    std::vector<int64_t> node_refs;
    std::unordered_map<std::string, std::string> tags;
};

struct Relation {
    int64_t id;

    struct Member {
        std::string type;
        int64_t ref;
        std::string role;
    };

    std::vector<Member> members;
    std::unordered_map<std::string, std::string> tags;
};

struct OSMData {
    std::unordered_map<int64_t, Node> nodes;
    std::unordered_map<int64_t, Way> ways;
    std::unordered_map<int64_t, Relation> relations;

    std::vector<std::pair<double, double>> get_way_coords(const Way& way) const {
        std::vector<std::pair<double, double>> coords;
        for (auto ref : way.node_refs) {
            auto it = nodes.find(ref);
            if (it != nodes.end()) {
                coords.emplace_back(it->second.lat, it->second.lon);
            }
        }
        return coords;
    }

    struct BoundingBox {
        double min_lat, max_lat, min_lon, max_lon;
    };

    BoundingBox bounds() const {
        if (nodes.empty()) {
            return {0, 0, 0, 0};
        }
        double min_lat = nodes.begin()->second.lat;
        double max_lat = min_lat;
        double min_lon = nodes.begin()->second.lon;
        double max_lon = min_lon;
        for (const auto& [id, node] : nodes) {
            if (node.lat < min_lat) min_lat = node.lat;
            if (node.lat > max_lat) max_lat = node.lat;
            if (node.lon < min_lon) min_lon = node.lon;
            if (node.lon > max_lon) max_lon = node.lon;
        }
        return {min_lat, max_lat, min_lon, max_lon};
    }
};
