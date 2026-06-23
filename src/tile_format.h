#pragma once
#include "image.h"
#include <string>

// Write a compact TMR tile format (palette + RLE) instead of PPM.
// Produces 5-30× smaller files than PPM for typical map tiles.
// Format: "TILE" magic, w:u16, h:u16, palette_size:u8,
//         palette[palette_size * 3], scanlines with RLE.
bool write_tmr_tile(const Image& img, const std::string& path);
