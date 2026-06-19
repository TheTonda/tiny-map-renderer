# Tiny Map Renderer — Implementation Plan

> **For Hermes:** Execute each task via `opencode run` in the project directory.
> Each task is self-contained and small enough for one OpenCode session.
> After each task, verify the build + test passes before proceeding to the next.

**Goal:** Build a from-scratch software map renderer in C++ that reads OSM XML
and renders styled map tiles to PPM images. No external graphics dependencies.

**Architecture:** Pure software pipeline:
  OSM XML → data model → style rules → projection → clipping → rasterization → PPM

**Tech Stack:** C++23, GCC, Make, no external libs (stdlib only)

**Project root:** `~/projects/tiny-map-renderer/`

**Existing files:** `tile_math.h` (projection + tile math, already tested)

---

## Task 1: Project skeleton + Makefile + Image class

**Objective:** Set up build system and create the Image (pixel buffer) class with PPM output.

**Files:**
- Create: `Makefile`
- Create: `src/image.h`
- Create: `src/image.cpp`
- Create: `tests/test_image.cpp`

**Requirements:**

`src/image.h` — Image class:
- `int width, height` fields
- `std::vector<uint32_t>` pixels (RGBA, 8 bits per channel, packed as 0xRRGGBBAA)
- Constructor `Image(int w, int h)` — fills with white (0xFFFFFFFF)
- `void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a=255)`
- `void set_pixel(int x, int y, uint32_t rgba)` — overload
- `uint32_t get_pixel(int x, int y) const`
- `void fill(uint8_t r, uint8_t g, uint8_t b)` — fill entire image
- `bool write_ppm(const std::string& path) const` — write binary PPM (P6, RGB only, drop alpha)
- Out-of-bounds set_pixel calls are silently ignored (no crash)

`Makefile`:
- `make` builds all tests
- `make test_image` builds the image test
- `make clean` removes binaries
- Use `g++ -std=c++23 -O2 -Wall -Wextra`
- Include paths: `-Isrc`

`tests/test_image.cpp`:
- Create 4x4 image, fill with red, write to /tmp/test_image.ppm
- Set individual pixels, verify get_pixel returns correct values
- Test out-of-bounds doesn't crash
- Print "PASS" or "FAIL" for each check
- `main()` returns 0 on all pass, 1 on any fail

**Verify:**
```
cd ~/projects/tiny-map-renderer
make test_image && ./test_image
```
Expected: all PASS, /tmp/test_image.ppm exists

---

## Task 2: Bresenham line rasterization

**Objective:** Draw lines into an Image using Bresenham's algorithm.

**Files:**
- Create: `src/rasterizer.h`
- Create: `src/rasterizer.cpp`
- Create: `tests/test_rasterizer.cpp`

**Requirements:**

`src/rasterizer.h` — free functions in a namespace `raster`:
- `void draw_line(Image& img, int x0, int y0, int x1, int y1, uint32_t color)`
  - Bresenham's line algorithm, handles all octants
- `void draw_line(Image& img, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)`
  — convenience overload

`src/rasterizer.cpp` — implement the above. Link against image.h.

`tests/test_rasterizer.cpp`:
- Create 100x100 image, draw a horizontal line (10,50)-(90,50), verify pixels at (10,50) and (90,50) are set
- Draw a vertical line (50,10)-(50,90), verify endpoints
- Draw a diagonal (10,10)-(90,90), verify midpoint (50,50)
- Draw a steep line (50,10)-(55,90), verify it doesn't crash
- Write result to /tmp/test_lines.ppm
- Print PASS/FAIL

**Verify:**
```
make test_rasterizer && ./test_rasterizer
```

---

## Task 3: Scanline polygon fill

**Objective:** Fill convex and concave polygons using the scanline fill algorithm.

**Files:**
- Modify: `src/rasterizer.h` (add function declaration)
- Modify: `src/rasterizer.cpp` (add implementation)
- Create: `tests/test_polyfill.cpp`

**Requirements:**

Add to `raster` namespace:
- `void fill_polygon(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color)`
  — scanline polygon fill. Works for concave polygons. Uses even-odd rule.
  — points is a vector of (x,y) vertex pairs, polygon is implicitly closed (last→first).

`tests/test_polyfill.cpp`:
- Fill a square: (10,10),(90,10),(90,90),(10,90) — verify center pixel (50,50) is filled
- Fill a triangle: (50,10),(10,90),(90,90) — verify (50,60) is filled, (50,5) is not
- Fill a concave "plus" shape — verify center filled, corner not
- Write to /tmp/test_polyfill.ppm
- Print PASS/FAIL

**Verify:**
```
make test_polyfill && ./test_polyfill
```

---

## Task 4: Thick line drawing (stroked polylines)

**Objective:** Draw polylines with configurable thickness — needed for roads.

**Files:**
- Modify: `src/rasterizer.h`
- Modify: `src/rasterizer.cpp`
- Create: `tests/test_thick_line.cpp`

**Requirements:**

Add to `raster` namespace:
- `void draw_polyline(Image& img, const std::vector<std::pair<int,int>>& points, uint32_t color, int thickness)`
  — Draws connected line segments with the given thickness.
  — For thickness=1, use Bresenham.
  — For thickness>1, draw filled circles at each point and thick line segments
    (simple approach: for each pixel on the Bresenham line, fill a filled disk of radius thickness/2).
- `void draw_line_thick(Image& img, int x0, int y0, int x1, int y1, uint32_t color, int thickness)`
  — single thick line segment

Add helper:
- `void fill_disk(Image& img, int cx, int cy, int radius, uint32_t color)`

`tests/test_thick_line.cpp`:
- Draw a thick horizontal line (10,50)-(90,50) thickness=5, verify pixel at (50,52) is set (within radius)
- Draw a polyline with 3 points forming an L-shape, thickness=3
- Write to /tmp/test_thick.ppm
- Print PASS/FAIL

**Verify:**
```
make test_thick_line && ./test_thick_line
```

---

## Task 5: Cohen-Sutherland line clipping

**Objective:** Clip line segments to a rectangular viewport. Essential for rendering — geometry outside the tile must be clipped before rasterization.

**Files:**
- Create: `src/clip.h`
- Create: `src/clip.cpp`
- Create: `tests/test_clip.cpp`

**Requirements:**

`src/clip.h`:
- `struct Rect { double x, y, w, h; }` — x,y is top-left, w,h are dimensions
- `bool clip_line_cohen_sutherland(double& x0, double& y0, double& x1, double& y1, const Rect& clip)`
  — Modifies x0,y0,x1,y1 in place. Returns true if any part of the line is visible, false if entirely outside.
- `bool clip_line_cohen_sutherland_int(int& x0, int& y0, int& x1, int& y1, int xmin, int ymin, int xmax, int ymax)`
  — Integer version for pixel-space clipping

`tests/test_clip.cpp`:
- Line fully inside → returns true, unchanged
- Line fully outside → returns false
- Line crossing left edge → clipped, x0 == xmin
- Line crossing two edges (corner) → clipped correctly
- Diagonal line partially visible → correct endpoints
- Print PASS/FAIL

**Verify:**
```
make test_clip && ./test_clip
```

---

## Task 6: OSM data model

**Objective:** Define the core data structures for OSM elements.

**Files:**
- Create: `src/osm_model.h`
- Create: `tests/test_osm_model.cpp`

**Requirements:**

`src/osm_model.h`:
```cpp
struct Node {
    int64_t id;
    double lat;
    double lon;
};

struct Way {
    int64_t id;
    std::vector<int64_t> node_refs;  // references to Node.id
    std::unordered_map<std::string, std::string> tags;
};

struct Relation {
    int64_t id;
    struct Member {
        std::string type;  // "node", "way", "relation"
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

    // Convenience: resolve a way's node_refs to actual lat/lon coordinates
    std::vector<std::pair<double,double>> get_way_coords(const Way& way) const;

    // Bounding box of all data
    struct BoundingBox { double min_lat, max_lat, min_lon, max_lon; };
    BoundingBox bounds() const;
};
```

`tests/test_osm_model.cpp`:
- Create a few nodes and ways manually, verify get_way_coords returns correct coordinates
- Test bounds() with known nodes
- Test tags access
- Print PASS/FAIL

**Verify:**
```
make test_osm_model && ./test_osm_model
```

---

## Task 7: OSM XML parser

**Objective:** Parse .osm XML files into OSMData. No external XML library — write a simple state-machine parser using std::regex or manual tag parsing.

**Files:**
- Create: `src/osm_parser.h`
- Create: `src/osm_parser.cpp`
- Create: `tests/test_data/minimal.osm`
- Create: `tests/test_osm_parser.cpp`

**Requirements:**

`src/osm_parser.h`:
- `OSMData parse_osm_xml(const std::string& filepath)`
  — Parses .osm XML, returns OSMData
  — Handles `<node>`, `<way>`, `<relation>`, `<nd>`, `<member>`, `<tag>`, `<bounds>`
  — Throws std::runtime_error on file not found or parse error

`tests/test_data/minimal.osm` — a tiny OSM XML with:
- 4-5 nodes forming a small road
- 1 way referencing those nodes with highway=residential tag
- bounds element

`tests/test_osm_parser.cpp`:
- Parse minimal.osm
- Verify correct number of nodes and ways parsed
- Verify node coordinates are correct
- Verify way tags are correct (highway=residential)
- Verify get_way_coords returns the right number of coordinate pairs
- Print PASS/FAIL

**Verify:**
```
make test_osm_parser && ./test_osm_parser
```

---

## Task 8: Style engine

**Objective:** Map OSM tags to visual styles (color, width, fill).

**Files:**
- Create: `src/style.h`
- Create: `src/style.cpp`
- Create: `tests/test_style.cpp`

**Requirements:**

`src/style.h`:
```cpp
struct Style {
    uint32_t color;      // line or fill color, RGBA
    int width;           // line thickness in pixels (0 for polygons = fill only)
    bool fill;           // true for area features (buildings, landuse)
    int z_order;         // render order (lower = drawn first = background)
};

class StyleEngine {
public:
    StyleEngine();
    // Returns the style for a way based on its tags, or std::nullopt if no rule matches
    std::optional<Style> style_for_way(const std::unordered_map<std::string,std::string>& tags) const;
private:
    struct Rule {
        std::string key;
        std::string value;  // empty = match any value for this key
        Style style;
    };
    std::vector<Rule> rules;
};
```

Default rules (populate in constructor):
- highway=motorway → color 0xE892A2FF (pinkish), width 6, z_order 10
- highway=trunk → 0xFCD6A4FF (orange), width 5, z_order 9
- highway=primary → 0xF9B29CFF, width 4, z_order 8
- highway=secondary → 0xFDBF6FFF, width 3, z_order 7
- highway=residential → 0xBBBBBBFF, width 2, z_order 5
- highway=footway → 0x999999FF, width 1, z_order 3
- building=* → 0xD9D0C9FF, fill=true, width 0, z_order 2
- landuse=forest → 0xAEDCA3FF, fill=true, width 0, z_order 1
- waterway=* → 0x7FC5E7FF, width 2, z_order 4
- natural=water → 0x7FC5E7FF, fill=true, width 0, z_order 1

`tests/test_style.cpp`:
- Create StyleEngine, query with highway=primary tags, verify color and width
- Query with building=yes, verify fill=true
- Query with unknown tags, verify nullopt returned
- Query with highway=service (not in rules but highway key exists), verify nullopt
- Print PASS/FAIL

**Verify:**
```
make test_style && ./test_style
```

---

## Task 9: Viewport renderer — combine everything

**Objective:** The core renderer that ties projection, clipping, styling, and rasterization together. Given OSMData + a viewport (lat/lon bounds) + image dimensions, produce a rendered map image.

**Files:**
- Create: `src/renderer.h`
- Create: `src/renderer.cpp`
- Create: `tests/test_renderer.cpp`

**Requirements:**

`src/renderer.h`:
```cpp
struct Viewport {
    double center_lat;
    double center_lon;
    int zoom;           // tile zoom level (determines scale)
    int width;          // output image dimensions
    int height;
};

class Renderer {
public:
    Renderer(const OSMData& data);
    Image render(const Viewport& vp) const;
private:
    const OSMData& data_;
    StyleEngine style_;

    // Project lat/lon to pixel coordinates within the viewport
    std::pair<int,int> project(double lat, double lon, const Viewport& vp) const;

    // Check if a way is within or intersects the viewport bounds
    bool way_in_viewport(const Way& way, const Viewport& vp) const;
};
```

Rendering algorithm in `render()`:
1. Compute the viewport's lat/lon bounds from center + zoom + image dimensions
   (use TileMath to determine pixel scale: at zoom z, 1 tile = 256px covers a known lat/lon range)
2. Sort all ways by z_order (ascending = background first)
3. For each way:
   a. Get its style; skip if no style matches
   b. Project all node coordinates to pixel space
   c. Clip the polyline/polygon to the image bounds (using Cohen-Sutherland per segment)
   d. Rasterize: fill_polygon if style.fill, draw_polyline if not
4. Return the Image

`tests/test_renderer.cpp`:
- Build a small OSMData manually with a few roads and a building
- Create a Viewport centered on the data
- Render to Image, write to /tmp/test_render.ppm
- Verify the image is not all white (some pixels are colored)
- Verify specific pixels are the expected road color
- Print PASS/FAIL

**Verify:**
```
make test_renderer && ./test_renderer
```

---

## Task 10: CLI entry point + full pipeline

**Objective:** Create a CLI tool that takes an OSM file + coordinates and outputs a rendered PPM map.

**Files:**
- Create: `src/main.cpp`
- Modify: `Makefile` (add `make tiny-map` target)

**Requirements:**

`src/main.cpp`:
- Usage: `./tiny-map <osm-file> <lat> <lon> <zoom> <width> <height> <output.ppm>`
- Parse OSM file, set up viewport, render, write PPM
- Print progress to stderr: "Parsed N nodes, M ways", "Rendering...", "Wrote output.ppm"
- Handle errors gracefully (file not found, etc.)

`Makefile`:
- Add target `tiny-map` that compiles src/main.cpp + all modules
- `make` builds everything including tests

**Verify:**
```
cd ~/projects/tiny-map-renderer
make tiny-map
./tiny-map tests/test_data/minimal.osm 48.8584 2.2945 14 512 512 /tmp/map.ppm
```
Expected: /tmp/map.ppm exists, shows some rendered content (not all white)

---

## Task 11: Real OSM data test + improvements

**Objective:** Download a real .osm extract, render it, and verify the output looks like a map.

**Files:**
- Create: `scripts/download_sample.sh` — downloads a small area from OpenStreetMap API
- Create: `tests/test_data/sample.osm` — a real OSM extract (small village/town center)

**Requirements:**

`scripts/download_sample.sh`:
- Uses curl to download from https://api.openstreetmap.org/api/0.6/map?bbox=...
- Downloads a ~0.01° × 0.01° area (small village)
- Saves to tests/test_data/sample.osm

**Verify:**
```
cd ~/projects/tiny-map-renderer
bash scripts/download_sample.sh
make tiny-map
./tiny-map tests/test_data/sample.osm <lat> <lon> 15 1024 1024 /tmp/real_map.ppm
```
Expected: /tmp/real_map.ppm shows roads, buildings, etc. — looks like a map

---

## Dependency Graph

```
Task 1 (Image) ──┬── Task 2 (Lines) ── Task 4 (Thick lines) ──┐
                  └── Task 3 (PolyFill) ────────────────────────┤
                                                                  ├── Task 9 (Renderer)
Task 5 (Clip) ───────────────────────────────────────────────────┤
                                                                  │
Task 6 (OSM Model) ─── Task 7 (Parser) ──────── Task 8 (Style) ──┘
                                                                       │
                                                                  Task 10 (CLI)
                                                                       │
                                                                  Task 11 (Real data)
```

Tasks 1-8 can partly run in sequence with some parallelism. But for OpenCode, run sequentially to keep it simple.

## Execution Notes

- Run each task as: `opencode run '<task prompt>'` in `~/projects/tiny-map-renderer/`
- The task prompt should include the full task spec from above
- After each task: verify build + test passes before proceeding
- If a task fails, debug and retry before moving on
- Commit after each successful task: `git add -A && git commit -m "task N: <description>"`
