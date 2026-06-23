#pragma once
#include "osm_model.h"
#include <string>

// Forward declaration
struct RenderData;

// --- v1 format (compatible, slower, full OSMData) ---
// Serialize OSMData to a compact binary format (~6× smaller than XML).
bool write_osm_binary(const OSMData& data, const std::string& path);

// Deserialize from v1 binary format via mmap.
OSMData read_osm_binary(const std::string& path);

// --- v2 format (pre-projected, grid-indexed, RenderData) ---
// Compile OSMData to v2 with pre-projected coords + spatial grid.
// This is the format used for 60fps rendering.
bool write_osm_binary_v2(const OSMData& data, const std::string& path);

// Load v2 binary format into compact RenderData.
// Returns empty RenderData on failure.
RenderData read_render_data(const std::string& path);
