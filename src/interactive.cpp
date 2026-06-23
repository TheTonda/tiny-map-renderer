#include "osm_parser.h"
#include "osm_binary.h"
#include "render_data.h"
#include "renderer.h"
#include "tile_math.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

static constexpr int DEFAULT_WIDTH = 1024;
static constexpr int DEFAULT_HEIGHT = 768;
static constexpr int MIN_ZOOM = 0;
static constexpr int MAX_ZOOM = 19;
static constexpr int PAN_PIXELS = 50;
static constexpr int TILE_SIZE = 256;
static constexpr int MAX_CACHED_TILES = 128;

struct TileKey {
    int64_t x, y;
    int z;
    bool operator==(const TileKey& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct TileKeyHash {
    size_t operator()(const TileKey& k) const {
        return static_cast<size_t>((k.x * 31 + k.y) * 31 + k.z);
    }
};

struct CachedTile {
    Image img;
    int64_t frame_id;
};

static void set_title(SDL_Window* window, double lat, double lon, int zoom) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "Tiny Map Renderer - %.4f, %.4f, z=%d", lat, lon, zoom);
    SDL_SetWindowTitle(window, buf);
}

static double clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

static bool update_texture(SDL_Texture*& texture, SDL_Renderer* sdl_renderer,
                           const Image& img) {
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING, img.width, img.height);
    if (!texture) return false;
    SDL_UpdateTexture(texture, nullptr, img.pixels.data(), img.width * 4);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file.tmr|file.osm> [lat] [lon] [zoom]\n", argv[0]);
        return 1;
    }

    std::string file_path = argv[1];

    // Load data — prefer v2 for speed
    RenderData rd;
    OSMData osm;
    bool use_v2 = false;

    // Peek magic to detect v2
    {
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd != -1) {
            char magic[4];
            if (read(fd, magic, 4) == 4 && std::memcmp(magic, "TMR2", 4) == 0) use_v2 = true;
            close(fd);
        }
    }

    if (use_v2) {
        rd = read_render_data(file_path);
        if (rd.nodes.empty()) { std::fprintf(stderr, "Failed to load v2\n"); return 1; }
        std::fprintf(stderr, "Loaded v2: %zu nodes, %zu ways, %dx%d grid\n",
            rd.nodes.size(), rd.ways.size(), rd.grid_cols, rd.grid_rows);
    } else {
        try {
            size_t len = file_path.size();
            if (len >= 4 && file_path.compare(len - 4, 4, ".tmr") == 0)
                osm = read_osm_binary(file_path);
            else
                osm = parse_osm_xml(file_path);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error: %s\n", e.what());
            return 1;
        }
        std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", osm.nodes.size(), osm.ways.size());
    }

    auto bounds = use_v2 ? OSMData::BoundingBox{0,0,0,0} : osm.bounds();

    double center_lat, center_lon;
    int zoom;
    if (argc >= 4) {
        center_lat = std::stod(argv[2]);
        center_lon = std::stod(argv[3]);
    } else if (use_v2) {
        center_lat = 48.8584; center_lon = 2.2945;
    } else {
        center_lat = (bounds.min_lat + bounds.max_lat) / 2.0;
        center_lon = (bounds.min_lon + bounds.max_lon) / 2.0;
    }
    zoom = (argc >= 5) ? std::stoi(argv[4]) : 15;
    zoom = clamp(zoom, MIN_ZOOM, MAX_ZOOM);

    int vp_width = DEFAULT_WIDTH, vp_height = DEFAULT_HEIGHT;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed\n"); return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Tiny Map Renderer",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        vp_width, vp_height, SDL_WINDOW_RESIZABLE);
    if (!window) { SDL_Quit(); return 1; }

    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    set_title(window, center_lat, center_lon, zoom);

    // Tile cache
    std::unordered_map<TileKey, CachedTile, TileKeyHash> tile_cache;
    int64_t frame_counter = 0;

    SDL_Texture* texture = nullptr;
    bool dirty = true;
    bool running = true;
    bool dragging = false;
    int last_mouse_x = 0, last_mouse_y = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false; break;

            case SDL_KEYDOWN:
                {
                    int cx = static_cast<int>(TileMath::lon_to_world_px(center_lon, zoom));
                    int cy = static_cast<int>(TileMath::lat_to_world_py(center_lat, zoom));
                    switch (event.key.keysym.sym) {
                    case SDLK_q: case SDLK_ESCAPE: running = false; break;
                    case SDLK_UP: case SDLK_w:    cy -= PAN_PIXELS; dirty = true; break;
                    case SDLK_DOWN: case SDLK_s:   cy += PAN_PIXELS; dirty = true; break;
                    case SDLK_LEFT: case SDLK_a:   cx -= PAN_PIXELS; dirty = true; break;
                    case SDLK_RIGHT: case SDLK_d:  cx += PAN_PIXELS; dirty = true; break;
                    case SDLK_EQUALS: case SDLK_PLUS:
                        zoom = clamp(zoom + 1, MIN_ZOOM, MAX_ZOOM); dirty = true; break;
                    case SDLK_MINUS:
                        zoom = clamp(zoom - 1, MIN_ZOOM, MAX_ZOOM); dirty = true; break;
                    }
                    if (dirty && event.key.keysym.sym != SDLK_q && event.key.keysym.sym != SDLK_ESCAPE) {
                        center_lon = TileMath::world_px_to_lon(cx, zoom);
                        center_lat = TileMath::world_py_to_lat(cy, zoom);
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging = true;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) dragging = false;
                break;

            case SDL_MOUSEMOTION:
                if (dragging) {
                    int dx = event.motion.x - last_mouse_x;
                    int dy = event.motion.y - last_mouse_y;
                    if (dx != 0 || dy != 0) {
                        int cx = static_cast<int>(TileMath::lon_to_world_px(center_lon, zoom));
                        int cy = static_cast<int>(TileMath::lat_to_world_py(center_lat, zoom));
                        cx -= dx; cy -= dy;
                        center_lon = TileMath::world_px_to_lon(cx, zoom);
                        center_lat = TileMath::world_py_to_lat(cy, zoom);
                        last_mouse_x = event.motion.x;
                        last_mouse_y = event.motion.y;
                        dirty = true;
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    int mx, my; SDL_GetMouseState(&mx, &my);
                    double mouse_wx = TileMath::lon_to_world_px(center_lon, zoom) + (mx - vp_width / 2.0);
                    double mouse_wy = TileMath::lat_to_world_py(center_lat, zoom) + (my - vp_height / 2.0);
                    double mouse_lat = TileMath::world_py_to_lat(mouse_wy, zoom);
                    double mouse_lon = TileMath::world_px_to_lon(mouse_wx, zoom);
                    int new_zoom = clamp(zoom + event.wheel.y, MIN_ZOOM, MAX_ZOOM);
                    if (new_zoom != zoom) {
                        zoom = new_zoom;
                        double new_cx = TileMath::lon_to_world_px(mouse_lon, zoom) - (mx - vp_width / 2.0);
                        double new_cy = TileMath::lat_to_world_py(mouse_lat, zoom) - (my - vp_height / 2.0);
                        center_lon = TileMath::world_px_to_lon(new_cx, zoom);
                        center_lat = TileMath::world_py_to_lat(new_cy, zoom);
                        dirty = true;
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    vp_width = event.window.data1;
                    vp_height = event.window.data2;
                    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
                    dirty = true;
                }
                break;
            }
        }

        if (dirty) {
            set_title(window, center_lat, center_lon, zoom);
            ++frame_counter;

            // Compute viewport world-pixel bounds
            double vp_cx = TileMath::lon_to_world_px(center_lon, zoom);
            double vp_cy = TileMath::lat_to_world_py(center_lat, zoom);
            double vp_x0 = vp_cx - vp_width / 2.0;
            double vp_y0 = vp_cy - vp_height / 2.0;
            double vp_x1 = vp_cx + vp_width / 2.0;
            double vp_y1 = vp_cy + vp_height / 2.0;

            // Determine tiles overlapping viewport
            int tx0 = static_cast<int>(std::floor(vp_x0 / TILE_SIZE));
            int ty0 = static_cast<int>(std::floor(vp_y0 / TILE_SIZE));
            int tx1 = static_cast<int>(std::floor((vp_x1 - 1e-9) / TILE_SIZE));
            int ty1 = static_cast<int>(std::floor((vp_y1 - 1e-9) / TILE_SIZE));

            // LRU eviction: clear stale tiles periodically
            if (tile_cache.size() > MAX_CACHED_TILES) {
                // Find oldest tile
                int64_t oldest = INT64_MAX;
                TileKey oldest_key{};
                for (const auto& [key, ct] : tile_cache) {
                    if (ct.frame_id < oldest) { oldest = ct.frame_id; oldest_key = key; }
                }
                if (oldest < INT64_MAX) tile_cache.erase(oldest_key);
            }

            // Ensure all visible tiles are cached
            for (int ty = ty0; ty <= ty1; ++ty) {
                for (int tx = tx0; tx <= tx1; ++tx) {
                    TileKey key{tx, ty, zoom};
                    if (tile_cache.find(key) == tile_cache.end()) {
                        double tile_cx = (tx + 0.5) * TILE_SIZE;
                        double tile_cy = (ty + 0.5) * TILE_SIZE;
                        double tile_lat = TileMath::world_py_to_lat(tile_cy, zoom);
                        double tile_lon = TileMath::world_px_to_lon(tile_cx, zoom);

                        Image tile_img(TILE_SIZE, TILE_SIZE);
                        if (use_v2) {
                            tile_img = render_v2(rd, tile_lat, tile_lon, zoom, TILE_SIZE, TILE_SIZE);
                        } else {
                            Viewport tvp{tile_lat, tile_lon, zoom, TILE_SIZE, TILE_SIZE};
                            Renderer renderer(osm);
                            tile_img = renderer.render(tvp);
                        }
                        tile_cache.insert_or_assign(key, CachedTile{std::move(tile_img), frame_counter});
                    }
                }
            }

            // Build viewport image from cached tiles
            Image vp_img(vp_width, vp_height);
            vp_img.fill(0xF5, 0xF5, 0xF5);

            for (int ty = ty0; ty <= ty1; ++ty) {
                for (int tx = tx0; tx <= tx1; ++tx) {
                    TileKey key{tx, ty, zoom};
                    auto it = tile_cache.find(key);
                    if (it == tile_cache.end()) continue;
                    it->second.frame_id = frame_counter; // mark as used

                    const Image& tile = it->second.img;
                    // Screen position of tile's top-left corner
                    int sx0 = static_cast<int>(tx * TILE_SIZE - vp_x0);
                    int sy0 = static_cast<int>(ty * TILE_SIZE - vp_y0);

                    // Blit: copy tile pixels to viewport image (only visible portion)
                    int copy_x0 = std::max(0, -sx0);
                    int copy_y0 = std::max(0, -sy0);
                    int copy_x1 = std::min(TILE_SIZE, vp_width - sx0);
                    int copy_y1 = std::min(TILE_SIZE, vp_height - sy0);

                    for (int py = copy_y0; py < copy_y1; ++py) {
                        int screen_y = sy0 + py;
                        if (screen_y < 0 || screen_y >= vp_height) continue;
                        for (int px = copy_x0; px < copy_x1; ++px) {
                            int screen_x = sx0 + px;
                            if (screen_x < 0 || screen_x >= vp_width) continue;
                            vp_img.pixels[screen_y * vp_width + screen_x] = tile.pixels[py * TILE_SIZE + px];
                        }
                    }
                }
            }

            if (!update_texture(texture, sdl_renderer, vp_img)) {
                std::fprintf(stderr, "Failed to create texture\n");
                running = false;
            }
            dirty = false;
        }

        if (running && texture) {
            SDL_RenderClear(sdl_renderer);
            SDL_RenderCopy(sdl_renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(sdl_renderer);
        }
    }

    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
