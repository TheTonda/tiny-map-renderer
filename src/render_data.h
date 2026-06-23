#pragma once
#include "image.h"
#include "style.h"
#include <cstdint>
#include <vector>

// Compact render-ready data with pre-projected coordinates.
// Built by the compile step, loaded via mmap for instant rendering.
// No trig, no hash maps, no string comparisons at render time.

struct RenderData {
    // Pre-projected world-pixel coordinates at reference_zoom.
    // Indexed by node index (0..N-1). At zoom 20, world-pixels fit in i32 (max ~268M).
    // During rendering, project via: pixel = ((wx - cx) >> shift) + half_w.
    struct ProjNode {
        int32_t wx;  // world pixel X at reference_zoom (fits in i32 at z≤20)
        int32_t wy;  // world pixel Y at reference_zoom
    };
    std::vector<ProjNode> nodes;

    // Pre-filtered, pre-styled ways. Only ways that matched a style rule.
    struct RenderWay {
        int32_t min_wx, min_wy;  // axis-aligned bounding box at reference_zoom
        int32_t max_wx, max_wy;
        uint32_t refs_offset;    // offset into way_refs array
        uint16_t refs_count;
        Style    style;
        uint8_t  z_order;        // redundant with style.z_order, but packed for speed
    };
    std::vector<RenderWay> ways;

    // Packed node reference indices into the nodes array.
    std::vector<uint32_t> way_refs;

    // Spatial grid for fast viewport queries.
    // Grid is aligned to world-pixel space at reference_zoom,
    // sized to cover all node data bounds.
    int grid_cols, grid_rows;
    double grid_origin_wx, grid_origin_wy;   // top-left of grid in world-pixels
    double grid_cell_w, grid_cell_h;         // cell dimensions in world-pixels

    struct GridCell {
        uint32_t offset;  // into grid_ways array
        uint16_t count;   // number of way indices in this cell
    };
    std::vector<GridCell> grid;             // grid_cols × grid_rows cells
    std::vector<uint32_t> grid_ways;        // packed way indices per cell

    int reference_zoom;

    // Returns the list of grid cells that overlap a viewport bounding box.
    // viewport_bounds in world-pixel at reference_zoom.
    void query_grid(double vp_wx_min, double vp_wy_min,
                    double vp_wx_max, double vp_wy_max,
                    std::vector<uint32_t>& out_way_indices) const;
};

// Query the grid for ways that may be visible in the given viewport bounds.
// Bounds are in world-pixel space at reference_zoom.
inline void RenderData::query_grid(
    double vp_wx_min, double vp_wy_min,
    double vp_wx_max, double vp_wy_max,
    std::vector<uint32_t>& out_way_indices) const
{
    out_way_indices.clear();

    int cell_x0 = static_cast<int>((vp_wx_min - grid_origin_wx) / grid_cell_w);
    int cell_y0 = static_cast<int>((vp_wy_min - grid_origin_wy) / grid_cell_h);
    int cell_x1 = static_cast<int>((vp_wx_max - grid_origin_wx) / grid_cell_w);
    int cell_y1 = static_cast<int>((vp_wy_max - grid_origin_wy) / grid_cell_h);

    if (cell_x0 < 0) cell_x0 = 0;
    if (cell_y0 < 0) cell_y0 = 0;
    if (cell_x1 >= grid_cols) cell_x1 = grid_cols - 1;
    if (cell_y1 >= grid_rows) cell_y1 = grid_rows - 1;
    if (cell_x0 > cell_x1 || cell_y0 > cell_y1) return;

    for (int cy = cell_y0; cy <= cell_y1; ++cy) {
        for (int cx = cell_x0; cx <= cell_x1; ++cx) {
            const auto& cell = grid[cy * grid_cols + cx];
            for (uint16_t j = 0; j < cell.count; ++j) {
                out_way_indices.push_back(grid_ways[cell.offset + j]);
            }
        }
    }
}

// Render from pre-processed RenderData (v2 format).
// No trig, no hash lookups — all integer math with shift-based projection.
// Uses the spatial grid to only iterate ways overlapping the viewport.
Image render_v2(const RenderData& rd, double center_lat, double center_lon,
                int zoom, int width, int height);
