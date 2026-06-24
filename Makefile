CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra -Isrc -fdata-sections -ffunction-sections
LDFLAGS = -Wl,--gc-sections
SDL_CFLAGS = $(shell pkg-config --cflags sdl2)
SDL_LIBS = $(shell pkg-config --libs sdl2)

# Release builds: strip + no RTTI for smaller binaries
RELEASE_FLAGS = -fno-rtti
RELEASE_LDFLAGS = -Wl,--strip-all

all: test_image test_tile_math test_rasterizer test_polyfill test_thick_line test_clip test_osm_model test_osm_parser test_style test_renderer tiny-map map-viewer

test_image: tests/test_image.cpp src/image.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_rasterizer: tests/test_rasterizer.cpp src/rasterizer.cpp src/image.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_tile_math: test_tile_math.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_polyfill: tests/test_polyfill.cpp src/rasterizer.cpp src/image.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_thick_line: tests/test_thick_line.cpp src/rasterizer.cpp src/image.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_clip: tests/test_clip.cpp src/clip.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_osm_model: tests/test_osm_model.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_osm_parser: tests/test_osm_parser.cpp src/osm_parser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_style: tests/test_style.cpp src/style.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_renderer: tests/test_renderer.cpp src/renderer.cpp src/image.cpp src/rasterizer.cpp src/style.cpp src/clip.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

tiny-map: src/main.cpp src/renderer.cpp src/image.cpp src/rasterizer.cpp src/style.cpp src/clip.cpp src/osm_parser.cpp src/osm_binary.cpp src/tile_format.cpp src/osm_pbf.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(LDFLAGS) $(RELEASE_LDFLAGS) -o $@ $^ -lz

map-viewer: src/interactive.cpp src/renderer.cpp src/image.cpp src/rasterizer.cpp src/style.cpp src/clip.cpp src/osm_parser.cpp src/osm_binary.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(LDFLAGS) -o $@ $^ $(SDL_LIBS)

clean:
	rm -f test_image test_tile_math test_rasterizer test_polyfill test_thick_line test_clip test_osm_model test_osm_parser test_style test_renderer tiny-map map-viewer *.o
