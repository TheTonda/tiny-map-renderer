CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra -Isrc

all: test_image test_tile_math

test_image: tests/test_image.cpp src/image.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_tile_math: test_tile_math.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f test_image test_tile_math *.o
