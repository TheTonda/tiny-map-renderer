CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra -Isrc

all: test_image test_tile_math test_rasterizer test_polyfill test_thick_line test_clip

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

clean:
	rm -f test_image test_tile_math test_rasterizer test_polyfill test_thick_line test_clip *.o
