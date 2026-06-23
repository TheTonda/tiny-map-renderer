#pragma once
#include <cmath>
#include <cstdint>
#include <string>

// All tile math uses the standard Slippy Map tilenames convention:
//   https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
// TILE_SIZE = 256 pixels per tile.
// Zoom 0: the whole world is one 256x256 tile.
// Zoom z: the world is 2^z × 2^z tiles.

struct LatLon {
    double lat;  // degrees north (negative = south)
    double lon;  // degrees east (negative = west)
};

struct TileID {
    int64_t x;
    int64_t y;
    int     z;

    auto operator<=>(const TileID&) const = default;
};

struct Bounds {
    double north;
    double south;
    double east;
    double west;
};

struct TileMath {
    static constexpr int TILE_SIZE = 256;

    // --- Project lat/lon into the Web Mercator pixel grid at zoom z ---
    // The world at zoom z is (TILE_SIZE * 2^z) pixels wide and tall.
    // (0,0) is the top-left of the mercator map at ~85.05°N, -180°E.
    static double lon_to_world_px(double lon, int z) {
        return (lon + 180.0) / 360.0 * (TILE_SIZE << z);
    }

    static double lat_to_world_py(double lat, int z) {
        // Clamp to Mercator bounds (~85.05°) to avoid inf/NaN
        if (lat > 85.05) lat = 85.05;
        if (lat < -85.05) lat = -85.05;
        double s = std::sin(lat * M_PI / 180.0);
        // atanh(s) = 0.5 * log((1+s)/(1-s)) — 2 transcendentals vs old 3 (tan+cos+log)
        double merc_n = 0.5 * std::log((1.0 + s) / (1.0 - s));
        return (1.0 - merc_n / M_PI) / 2.0 * (TILE_SIZE << z);
    }

    // --- Inverse: pixel → lat/lon ---
    static double world_px_to_lon(double px, int z) {
        return px / (TILE_SIZE << z) * 360.0 - 180.0;
    }

    static double world_py_to_lat(double py, int z) {
        double merc_n = M_PI * (1.0 - 2.0 * py / (TILE_SIZE << z));
        return std::atan(std::sinh(merc_n)) * 180.0 / M_PI;
    }

    // --- Full round-trip ---
    static TileID latlon_to_tile(LatLon ll, int z) {
        return {
            .x = static_cast<int64_t>(lon_to_world_px(ll.lon, z) / TILE_SIZE),
            .y = static_cast<int64_t>(lat_to_world_py(ll.lat, z) / TILE_SIZE),
            .z = z,
        };
    }

    static Bounds tile_bounds(TileID t) {
        double n = world_py_to_lat(t.y * TILE_SIZE,           t.z);
        double s = world_py_to_lat((t.y + 1) * TILE_SIZE,     t.z);
        double e = world_px_to_lon((t.x + 1) * TILE_SIZE,     t.z);
        double w = world_px_to_lon(t.x * TILE_SIZE,           t.z);
        return {n, s, e, w};
    }

    // --- Pixel position *within* a specific tile ---
    static double pixel_in_tile_x(LatLon ll, TileID t) {
        return lon_to_world_px(ll.lon, t.z) - t.x * TILE_SIZE;
    }

    static double pixel_in_tile_y(LatLon ll, TileID t) {
        return lat_to_world_py(ll.lat, t.z) - t.y * TILE_SIZE;
    }

    // --- Inverse: pixel within a tile → lat/lon ---
    static LatLon tile_pixel_to_latlon(TileID t, double px, double py) {
        return {
            .lat = world_py_to_lat((t.y * TILE_SIZE) + py, t.z),
            .lon = world_px_to_lon((t.x * TILE_SIZE) + px, t.z),
        };
    }

    // --- Human-readable tile URL for OpenStreetMap ---
    static std::string osm_tile_url(TileID t) {
        return "https://tile.openstreetmap.org/"
             + std::to_string(t.z) + "/"
             + std::to_string(t.x) + "/"
             + std::to_string(t.y) + ".png";
    }
};
