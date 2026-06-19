// test_tile_math.cpp — compile with: g++ -std=c++23 -O2 -o test_tile_math test_tile_math.cpp
#include "tile_math.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <numbers>

// ---------------------------------------------------------------------------
// 1. Basic round-trip tests
// ---------------------------------------------------------------------------
int test_roundtrip() {
    int errors = 0;

    // Pick a few real-world spots
    LatLon spots[] = {
        { 51.1789, -1.8262 },   // Stonehenge
        { 48.8584,   2.2945 },  // Eiffel Tower
        { 27.1751,  78.0421 },  // Taj Mahal
        { 41.8902,  12.4922 },  // Colosseum
        { -33.8568, 151.2153 }, // Sydney Opera House
    };

    for (auto spot : spots) {
        for (int z = 0; z <= 18; z++) {
            auto tile = TileMath::latlon_to_tile(spot, z);

            double px = TileMath::pixel_in_tile_x(spot, tile);
            double py = TileMath::pixel_in_tile_y(spot, tile);

            auto back = TileMath::tile_pixel_to_latlon(tile, px, py);

            // Should be within ~0.001° (about 100m at equator)
            double dlat = std::abs(back.lat - spot.lat);
            double dlon = std::abs(back.lon - spot.lon);
            if (dlat > 0.001 || dlon > 0.001) {
                std::printf("  FAIL z=%d: (%g,%g) → tile %ld/%ld → (%g,%g)\n",
                           z, spot.lat, spot.lon, tile.x, tile.y, back.lat, back.lon);
                errors++;
            }
        }
    }
    return errors;
}

// ---------------------------------------------------------------------------
// 2. Tile bounds sanity: corners should make sense
// ---------------------------------------------------------------------------
int test_bounds() {
    int errors = 0;
    // Zoom 0: single tile covering the world
    auto b0 = TileMath::tile_bounds({0, 0, 0});
    // North should be ~85°, south ~-85°, east=180, west=-180
    if (std::abs(b0.north - 85.05) > 0.01) { std::printf("  FAIL zoom 0 north\n"); errors++; }
    if (std::abs(b0.south + 85.05) > 0.01) { std::printf("  FAIL zoom 0 south\n"); errors++; }
    if (std::abs(b0.east - 180.0) > 0.001) { std::printf("  FAIL zoom 0 east\n");  errors++; }
    if (std::abs(b0.west + 180.0) > 0.001) { std::printf("  FAIL zoom 0 west\n");  errors++; }

    // Zoom 10, tile (512, 512) — should be somewhere in Europe-ish area
    auto b10 = TileMath::tile_bounds({512, 512, 10});
    std::printf("  Tile (512, 512) z=10 bounds: N=%g S=%g E=%g W=%g\n",
               b10.north, b10.south, b10.east, b10.west);

    return errors;
}

// ---------------------------------------------------------------------------
// 3. PPM output: draw a tile grid with the Eiffel Tower marked
// ---------------------------------------------------------------------------
void draw_test_tile() {
    constexpr int W = 256;
    constexpr int H = 256;
    std::vector<uint8_t> pixels(W * H * 3, 255); // white background

    auto set_pixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        int idx = (y * W + x) * 3;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
    };

    // Pick the tile containing the Eiffel Tower at zoom 12
    LatLon eiffel = {48.8584, 2.2945};
    TileID tile = TileMath::latlon_to_tile(eiffel, 12);
    double cx = TileMath::pixel_in_tile_x(eiffel, tile);
    double cy = TileMath::pixel_in_tile_y(eiffel, tile);

    std::printf("  Eiffel Tower tile z=12: (%ld, %ld)\n", tile.x, tile.y);
    std::printf("  Pixel within tile: (%.1f, %.1f)\n", cx, cy);

    // Draw a crosshair at the Eiffel Tower position
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int px = static_cast<int>(cx) + dx;
            int py = static_cast<int>(cy) + dy;
            if (std::abs(dx) <= 1 || std::abs(dy) <= 1)
                set_pixel(px, py, 255, 0, 0);   // red cross
        }
    }

    // Draw a graticule (every 10 pixels, thin grey lines)
    for (int gx = 0; gx < W; gx += 10)
        for (int gy = 0; gy < H; gy++)
            set_pixel(gx, gy, 220, 220, 220);
    for (int gy = 0; gy < H; gy += 10)
        for (int gx = 0; gx < W; gx++)
            set_pixel(gx, gy, 220, 220, 220);

    // Draw the tile border
    for (int x = 0; x < W; x++) {
        set_pixel(x, 0, 0, 0, 0);
        set_pixel(x, H-1, 0, 0, 0);
    }
    for (int y = 0; y < H; y++) {
        set_pixel(0, y, 0, 0, 0);
        set_pixel(W-1, y, 0, 0, 0);
    }

    // Write PPM (netpbm format, ASCII for simplicity)
    std::FILE* f = std::fopen("/tmp/eiffel_tile.ppm", "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    std::fwrite(pixels.data(), 1, pixels.size(), f);
    std::fclose(f);
    std::printf("  Wrote /tmp/eiffel_tile.ppm\n");
}

// ---------------------------------------------------------------------------
// 4. Zoom level tour: show one spot at every zoom level
// ---------------------------------------------------------------------------
void zoom_tour() {
    LatLon home = {49.9, 14.4}; // roughly Prague
    std::printf("\n  Prague (49.9°N, 14.4°E) at each zoom:\n");
    for (int z = 0; z <= 14; z += 2) {
        auto t = TileMath::latlon_to_tile(home, z);
        auto b = TileMath::tile_bounds(t);
        double px = TileMath::pixel_in_tile_x(home, t);
        double py = TileMath::pixel_in_tile_y(home, t);
        std::printf("    z=%2d  tile (%5ld, %5ld)  pixel (%.0f, %.0f)"
                   "  bounds N=%.2f S=%.2f E=%.2f W=%.2f\n",
                   z, t.x, t.y, px, py, b.north, b.south, b.east, b.west);
    }
}

// ---------------------------------------------------------------------------
int main() {
    std::printf("=== TileMath round-trip tests ===\n");
    int err = test_roundtrip();
    std::printf("  Round-trip errors: %d\n\n", err);

    std::printf("=== Tile bounds ===\n");
    err += test_bounds();

    std::printf("\n=== Visual sanity ===\n");
    draw_test_tile();

    zoom_tour();

    std::printf("\n=== Done ===\n");
    return err ? 1 : 0;
}
