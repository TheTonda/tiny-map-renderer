#pragma once
#include "osm_model.h"
#include <string>

// Serialize OSMData to a compact binary format (~8× smaller than XML).
// Format is mmap-friendly: flat records with length-prefixed sections.
bool write_osm_binary(const OSMData& data, const std::string& path);

// Deserialize from compact binary format via mmap.
// Much faster than XML parsing — no string scanning, no transcendentals.
OSMData read_osm_binary(const std::string& path);
