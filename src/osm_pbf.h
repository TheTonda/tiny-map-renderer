#pragma once
#include "osm_model.h"
#include <string>

// Parse OSM PBF (Protocol Buffer Binary Format).
// Supports zlib-compressed blobs. Requires -lz linkage.
// Memory usage: processes blocks incrementally — does not hold entire file.
OSMData parse_osm_pbf(const std::string& filepath);
