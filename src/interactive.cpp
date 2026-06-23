#include "osm_parser.h"
#include "osm_binary.h"
#include "renderer.h"
#include "tile_math.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static constexpr int DEFAULT_WIDTH = 1024;
static constexpr int DEFAULT_HEIGHT = 768;
static constexpr int MIN_ZOOM = 0;
static constexpr int MAX_ZOOM = 19;
static constexpr int PAN_PIXELS = 50;

static void set_title(SDL_Window* window, double lat, double lon, int zoom) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "Tiny Map Renderer - %.4f, %.4f, z=%d", lat, lon, zoom);
    SDL_SetWindowTitle(window, buf);
}

static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

static bool update_texture(SDL_Texture*& texture, SDL_Renderer* sdl_renderer,
                           const Image& img) {
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ABGR8888,
                                SDL_TEXTUREACCESS_STREAMING, img.width, img.height);
    if (!texture) return false;
    SDL_UpdateTexture(texture, nullptr, img.pixels.data(), img.width * 4);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <osm-file> [lat] [lon] [zoom]\n", argv[0]);
        return 1;
    }

    std::string osm_file = argv[1];

    OSMData data;
    try {
        size_t len = osm_file.size();
        if (len >= 4 && osm_file.compare(len - 4, 4, ".tmr") == 0) {
            data = read_osm_binary(osm_file);
        } else {
            data = parse_osm_xml(osm_file);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error parsing OSM file: %s\n", e.what());
        return 1;
    }

    std::fprintf(stderr, "Parsed %zu nodes, %zu ways\n", data.nodes.size(), data.ways.size());

    auto bounds = data.bounds();

    Viewport vp;
    vp.width = DEFAULT_WIDTH;
    vp.height = DEFAULT_HEIGHT;

    if (argc >= 4) {
        vp.center_lat = std::stod(argv[2]);
        vp.center_lon = std::stod(argv[3]);
    } else {
        vp.center_lat = (bounds.min_lat + bounds.max_lat) / 2.0;
        vp.center_lon = (bounds.min_lon + bounds.max_lon) / 2.0;
    }

    if (argc >= 5) {
        vp.zoom = std::stoi(argv[4]);
    } else {
        vp.zoom = 15;
    }

    vp.zoom = clamp(vp.zoom, MIN_ZOOM, MAX_ZOOM);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Tiny Map Renderer",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        vp.width, vp.height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    set_title(window, vp.center_lat, vp.center_lon, vp.zoom);

    Renderer renderer(data);
    SDL_Texture* texture = nullptr;
    bool dirty = true;
    bool running = true;
    bool dragging = false;
    int last_mouse_x = 0;
    int last_mouse_y = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                {
                    double cx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom);
                    double cy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom);
                    switch (event.key.keysym.sym) {
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_UP:
                    case SDLK_w:
                        cy -= PAN_PIXELS;
                        dirty = true;
                        break;
                    case SDLK_DOWN:
                    case SDLK_s:
                        cy += PAN_PIXELS;
                        dirty = true;
                        break;
                    case SDLK_LEFT:
                    case SDLK_a:
                        cx -= PAN_PIXELS;
                        dirty = true;
                        break;
                    case SDLK_RIGHT:
                    case SDLK_d:
                        cx += PAN_PIXELS;
                        dirty = true;
                        break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                        vp.zoom = clamp(vp.zoom + 1, MIN_ZOOM, MAX_ZOOM);
                        dirty = true;
                        break;
                    case SDLK_MINUS:
                        vp.zoom = clamp(vp.zoom - 1, MIN_ZOOM, MAX_ZOOM);
                        dirty = true;
                        break;
                    }
                    if (dirty && event.key.keysym.sym != SDLK_q &&
                        event.key.keysym.sym != SDLK_ESCAPE) {
                        vp.center_lon = TileMath::world_px_to_lon(cx, vp.zoom);
                        vp.center_lat = TileMath::world_py_to_lat(cy, vp.zoom);
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
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging = false;
                }
                break;

            case SDL_MOUSEMOTION:
                if (dragging) {
                    int dx = event.motion.x - last_mouse_x;
                    int dy = event.motion.y - last_mouse_y;
                    if (dx != 0 || dy != 0) {
                        double cx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom);
                        double cy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom);
                        cx -= dx;
                        cy -= dy;
                        vp.center_lon = TileMath::world_px_to_lon(cx, vp.zoom);
                        vp.center_lat = TileMath::world_py_to_lat(cy, vp.zoom);
                        last_mouse_x = event.motion.x;
                        last_mouse_y = event.motion.y;
                        dirty = true;
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);

                    double mouse_wx = TileMath::lon_to_world_px(vp.center_lon, vp.zoom)
                                    + (mx - vp.width / 2.0);
                    double mouse_wy = TileMath::lat_to_world_py(vp.center_lat, vp.zoom)
                                    + (my - vp.height / 2.0);
                    double mouse_lat = TileMath::world_py_to_lat(mouse_wy, vp.zoom);
                    double mouse_lon = TileMath::world_px_to_lon(mouse_wx, vp.zoom);

                    int new_zoom = clamp(vp.zoom + event.wheel.y, MIN_ZOOM, MAX_ZOOM);
                    if (new_zoom != vp.zoom) {
                        vp.zoom = new_zoom;

                        double new_center_wx = TileMath::lon_to_world_px(mouse_lon, vp.zoom)
                                             - (mx - vp.width / 2.0);
                        double new_center_wy = TileMath::lat_to_world_py(mouse_lat, vp.zoom)
                                             - (my - vp.height / 2.0);
                        vp.center_lon = TileMath::world_px_to_lon(new_center_wx, vp.zoom);
                        vp.center_lat = TileMath::world_py_to_lat(new_center_wy, vp.zoom);
                        dirty = true;
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    vp.width = event.window.data1;
                    vp.height = event.window.data2;
                    if (texture) {
                        SDL_DestroyTexture(texture);
                        texture = nullptr;
                    }
                    dirty = true;
                }
                break;
            }
        }

        if (dirty) {
            set_title(window, vp.center_lat, vp.center_lon, vp.zoom);
            Image img = renderer.render(vp);
            if (!update_texture(texture, sdl_renderer, img)) {
                std::fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
                running = false;
                break;
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
