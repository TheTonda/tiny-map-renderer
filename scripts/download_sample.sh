#!/bin/bash
# Download a small area of OpenStreetMap data around the historic town center of
# Tábor, Czech Republic (Žižkovo náměstí). Center: 49.4138, 14.6581. Area: ~0.01° box.

set -euo pipefail

# Center: 49.4138, 14.6581 — Tábor (Žižkovo náměstí), Czech Republic
# Bounding box: ~0.01° × 0.01° (~1.1km × 0.7km)
WEST=14.652
SOUTH=49.409
EAST=14.663
NORTH=49.419

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_DIR/tests/test_data"
OUTPUT_FILE="$OUTPUT_DIR/sample.osm"

mkdir -p "$OUTPUT_DIR"

URL="https://api.openstreetmap.org/api/0.6/map?bbox=${WEST},${SOUTH},${EAST},${NORTH}"

echo "Downloading OSM data from: $URL"
echo "Saving to: $OUTPUT_FILE"

curl -sS -A "tiny-map-renderer/1.0 (demo project)" -o "$OUTPUT_FILE" "$URL"

if [ -s "$OUTPUT_FILE" ]; then
    echo "Download complete: $(wc -c < "$OUTPUT_FILE") bytes"
else
    echo "ERROR: Download failed or returned empty response"
    exit 1
fi
