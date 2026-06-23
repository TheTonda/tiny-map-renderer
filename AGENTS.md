# AGENTS.md — Tiny Map Renderer

## Build & test

- **Build everything** (tests + binaries): `make`
- **Build single target**: `make test_image`, `make tiny-map`, `make map-viewer`
- **Run a test**: `make <test_target> && ./<test_target>` (tests output PASS/FAIL to stdout, exit 0 on all-pass)
- There is no separate test runner, lint, formatter, or typecheck step. The Makefile is the sole config.
- Tests write output PPM files to `/tmp/` — visual inspection is manual.

## Architecture

A from-scratch software map renderer in C++23 with no external graphics libraries (stdlib only, `map-viewer` additionally requires SDL2).

Pipeline: OSM XML → osm_parser → OSMData → Renderer (projection + clip + style + rasterize) → Image → PPM/TTL

```
src/
  image.{h,cpp}        — Pixel buffer, RGBA packed as uint32_t, PPM export (P6 binary, buffered write), set_pixel_unsafe
  tile_math.h           — Mercator projection math (header-only), simplified trig (atanh via log)
  rasterizer.{h,cpp}    — Bresenham lines, polygon fill (pre-allocated int intersections, ceil-rounding), thick lines via polygon quads
  clip.{h,cpp}          — Cohen-Sutherland line clipping (double and int overloads)
  osm_model.h           — Node/Way/Relation/OSMData (header-only, methods inline)
  osm_parser.{h,cpp}    — mmap-based XML parser, hand-rolled memchr attribute scanner, from_chars numbers
  osm_binary.{h,cpp}    — Compact binary format (.tmr): ~6× smaller than XML, mmap-loaded, ~2× faster parse
  tile_format.{h,cpp}   — Palette + RLE tile output (.ttl): ~10× smaller than PPM
  style.{h,cpp}         — StyleEngine maps OSM tags → color/width/fill/z_order
  renderer.{h,cpp}      — Renderer: single-pass projection + culling, stable_sort by z_order, clipping + rasterization
  main.cpp              — CLI: ./tiny-map <file.osm|file.tmr> <lat> <lon> <zoom> <w> <h> <out.ppm|out.ttl>
                         — CLI: ./tiny-map --compile <file.osm>  (produces .tmr)
  interactive.cpp       — SDL2 interactive viewer: ./map-viewer <osm-file> [lat] [lon] [zoom]
```

## Key conventions

- **C++23** with g++ (`-std=c++23 -O2 -Wall -Wextra -Isrc`). No CMake, no autotools — raw Makefile.
- **Release binaries** (`tiny-map`, `map-viewer`) use extra flags: `-fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--strip-all` for minimal binary size.
- Colors are **RGBA** packed as `uint32_t` (0xRRGGBBAA), not ARGB. Alpha is the **low** byte.
- `Image::set_pixel` silently ignores out-of-bounds accesses. `set_pixel_unsafe` exists for internal clipped coordinates (no bounds check).
- `OSMData::bounds()` returns `{0,0,0,0}` for empty data, not a sentinel.
- `tile_math.h` is header-only; `test_tile_math.cpp` lives at the repo root (not in `tests/`) and compiles standalone.
- The `download_sample.sh` script downloads from the live OSM API (Tábor, Czech Republic, ~0.01°×0.01°) — requires network.
- `map-viewer` needs SDL2 installed (`pkg-config --cflags --libs sdl2`). It uses `SDL_PIXELFORMAT_ABGR8888` (ABGR, matching the RGBA-with-alpha-low packing).

## Performance notes

- The OSM parser uses `mmap` for zero-copy line scanning and a hand-rolled `memchr`-based attribute scanner (no `std::regex`).
- Number parsing uses `std::from_chars` (no `stoll`/`stod` intermediates).
- Hash maps are pre-reserved based on file-size heuristics to minimize rehash overhead.
- Thick lines are rendered as polygon quads (not per-pixel disks), using `std::ceil` for corner rounding to ensure full pixel coverage.
- `fill_polygon` pre-allocates the intersection vector once (no per-scanline allocation) and uses 64-bit fixed-point math.
- PPM output builds the entire pixel buffer in one allocation and writes with a single `fwrite` call.
- Visibility culling and coordinate projection are done in a single pass over each way's node_refs.

## Testing quirks

- `test_renderer` depends on `style.cpp` (not just style.h) because it needs the default rules instantiated.
- Tests are simple standalone `.cpp` files that print PASS/FAIL lines and return 0 or 1. No test framework.
- Sample OSM test data is in `tests/test_data/`: `minimal.osm` (hand-crafted), `sample.osm` (real data from OSM API).

## File layout oddities

- `test_tile_math.cpp` is at the repo root, not in `tests/`. All other tests are in `tests/`.
- Dependencies are inferred from the Makefile targets — each target's recipe lists the `.cpp` files needed. If adding a new module, update the relevant target's recipe line(s).
