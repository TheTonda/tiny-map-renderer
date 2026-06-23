#include "style.h"
#include <charconv>

StyleEngine::StyleEngine() {
    // OSM Carto-inspired style — field order: {color, casing_color, width, casing_width, fill, z_order}
    // Colors are RGBA packed as 0xRRGGBBAA (alpha low byte).
    // https://github.com/gravitystorm/openstreetmap-carto

    // ============================================================
    // z_order 0: Water & natural fills (background)
    // ============================================================
    rules.push_back({"natural", "water",      {0xB5D0D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"waterway", "riverbank", {0xB5D0D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "reservoir",  {0xB5D0D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "basin",      {0xB5D0D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "wetland",    {0xD5E5D5FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "glacier",    {0xDDEBFAFF, 0, 0, 0, true, 0}});

    // Green areas
    rules.push_back({"landuse", "forest",     {0xADD19EFF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "wood",       {0xADD19EFF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "park",       {0xC8F2C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "garden",     {0xC8F2C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "playground", {0xCCF2CCFF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "pitch",      {0x8AD38AFF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "sports_centre", {0x99D699FF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "stadium",    {0x99D699FF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "track",      {0x99D699FF, 0, 0, 0, true, 0}});
    rules.push_back({"leisure", "recreation_ground", {0xD2F0D2FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "recreation_ground", {0xD2F0D2FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "grass",      {0xCDF7C3FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "meadow",     {0xCDF7C3FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "grassland",  {0xCDF7C3FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "scrub",      {0xB5E0B5FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "heath",      {0xD6D999FF, 0, 0, 0, true, 0}});

    // Sand/beach
    rules.push_back({"natural", "beach",      {0xF1E9B6FF, 0, 0, 0, true, 0}});
    rules.push_back({"natural", "sand",       {0xF1E9B6FF, 0, 0, 0, true, 0}});

    // Urban landuse (subtle fills)
    rules.push_back({"landuse", "residential",  {0xE0DFDFFF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "commercial",   {0xEFC8C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "industrial",   {0xEBD6D2FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "retail",       {0xFFD6D1FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "farmland",     {0xEEF5D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "farmyard",     {0xEEF5D0FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "cemetery",     {0xA9D0A6FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "quarry",       {0xC5BEA6FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "brownfield",   {0xCDC5A6FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "construction", {0xCDC5A6FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "military",     {0xDFD5C5FF, 0, 0, 0, true, 0}});
    rules.push_back({"landuse", "garages",      {0xDFD5C5FF, 0, 0, 0, true, 0}});

    // Amenity areas
    rules.push_back({"amenity", "parking",     {0xF7EFB7FF, 0, 0, 0, true, 0}});
    rules.push_back({"amenity", "school",      {0xF0F0C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"amenity", "university",  {0xF0F0C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"amenity", "college",     {0xF0F0C8FF, 0, 0, 0, true, 0}});
    rules.push_back({"amenity", "hospital",    {0xF0D8D8FF, 0, 0, 0, true, 0}});

    // ============================================================
    // z_order 1: Buildings
    // ============================================================
    rules.push_back({"building", "residential", {0xD9C8B8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "house",       {0xD9C8B8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "apartments",  {0xD9C8B8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "terrace",     {0xD9C8B8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "detached",    {0xD9C8B8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "commercial",  {0xEBD6D6FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "industrial",  {0xEBD6D6FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "retail",      {0xFFD6D1FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "school",      {0xF0D8C8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "hospital",    {0xF0D8C8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "church",      {0xD4B8A8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "cathedral",   {0xD4B8A8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "mosque",      {0xD4B8A8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "temple",      {0xD4B8A8FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "garage",      {0xD9D0C9FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "shed",        {0xD9D0C9FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "roof",        {0xD5CCC5FF, 0, 0, 0, true, 1}});
    rules.push_back({"building", "*",           {0xD9D0C9FF, 0, 0, 0, true, 1}});

    // ============================================================
    // z_order 3: Railways (drawn below roads)
    // ============================================================
    rules.push_back({"railway", "rail",         {0xAAAAAAAA, 0, 2, 0, false, 3}});
    rules.push_back({"railway", "tram",         {0xAAAAAAAA, 0, 1, 0, false, 3}});
    rules.push_back({"railway", "light_rail",   {0xBBBBBBFF, 0, 1, 0, false, 3}});
    rules.push_back({"railway", "subway",       {0xBBBBBBFF, 0, 1, 0, false, 3}});
    rules.push_back({"railway", "narrow_gauge", {0xBBBBBBFF, 0, 1, 0, false, 3}});
    rules.push_back({"railway", "monorail",     {0xBBBBBBFF, 0, 1, 0, false, 3}});

    // ============================================================
    // z_order 4: Waterways (lines)
    // ============================================================
    rules.push_back({"waterway", "river",       {0xB5D0D0FF, 0, 3, 0, false, 4}});
    rules.push_back({"waterway", "canal",       {0xB5D0D0FF, 0, 2, 0, false, 4}});
    rules.push_back({"waterway", "stream",      {0xB5D0D0FF, 0, 1, 0, false, 4}});
    rules.push_back({"waterway", "drain",       {0xB5D0D0FF, 0, 1, 0, false, 4}});
    rules.push_back({"waterway", "ditch",       {0xB5D0D0FF, 0, 1, 0, false, 4}});

    // ============================================================
    // z_order 5-8: Minor paths & pedestrian
    // ============================================================
    rules.push_back({"highway", "path",         {0xAE6844FF, 0, 1, 0, false, 5}});
    rules.push_back({"highway", "footway",      {0xAE6844FF, 0, 1, 0, false, 5}});
    rules.push_back({"highway", "steps",        {0xFAB69BFF, 0, 1, 0, false, 5}});
    rules.push_back({"highway", "cycleway",     {0x58A2C4FF, 0, 1, 0, false, 6}});
    rules.push_back({"highway", "bridleway",    {0x99CC66FF, 0, 1, 0, false, 5}});

    rules.push_back({"highway", "track",        {0xC5A06BFF, 0, 1, 0, false, 7}});
    rules.push_back({"highway", "service",      {0xFFFFFFFF, 0, 1, 0, false, 8}});

    rules.push_back({"highway", "pedestrian",   {0xE5E0C2FF, 0, 2, 0, true,  6}});
    rules.push_back({"highway", "living_street",{0xDDDDDDFF, 0, 2, 0, false, 9}});

    // ============================================================
    // z_order 10-12: Surface streets (white with no casing)
    // ============================================================
    rules.push_back({"highway", "residential",   {0xFFFFFFFF, 0, 2, 0, false, 10}});
    rules.push_back({"highway", "unclassified",  {0xFFFFFFFF, 0, 2, 0, false, 10}});
    rules.push_back({"highway", "road",          {0xFFFFFFFF, 0, 2, 0, false, 10}});

    // ============================================================
    // z_order 11-15: Tertiary & secondary (with casing)
    // ============================================================
    rules.push_back({"highway", "tertiary",      {0xFFFFFFFF, 0xC5C5C5FF, 3, 2, false, 12}});
    rules.push_back({"highway", "tertiary_link", {0xFFFFFFFF, 0xC5C5C5FF, 2, 2, false, 12}});

    // Secondary — pale yellow fill, darker yellow casing
    rules.push_back({"highway", "secondary",      {0xFEFEC9FF, 0xD0D09EFF, 4, 2, false, 14}});
    rules.push_back({"highway", "secondary_link", {0xFEFEC9FF, 0xD0D09EFF, 3, 2, false, 14}});

    // ============================================================
    // z_order 16-18: Primary roads (orange fill, darker casing)
    // ============================================================
    rules.push_back({"highway", "primary",       {0xFDBF6FFF, 0xD99B5EFF, 5, 2, false, 17}});
    rules.push_back({"highway", "primary_link",  {0xFDBF6FFF, 0xD99B5EFF, 4, 2, false, 17}});

    // ============================================================
    // z_order 19-21: Trunk roads (pale orange, darker casing)
    // ============================================================
    rules.push_back({"highway", "trunk",         {0xFCD6A4FF, 0xD98F5EFF, 6, 2, false, 20}});
    rules.push_back({"highway", "trunk_link",    {0xFCD6A4FF, 0xD98F5EFF, 5, 2, false, 20}});

    // ============================================================
    // z_order 22-24: Motorways (pink-red, darker casing)
    // ============================================================
    rules.push_back({"highway", "motorway",      {0xE892A2FF, 0xC54E6EFF, 7, 3, false, 23}});
    rules.push_back({"highway", "motorway_link", {0xE892A2FF, 0xC54E6EFF, 5, 3, false, 23}});

    // ============================================================
    // z_order 25: Runways, bridges
    // ============================================================
    rules.push_back({"aeroway", "runway",        {0xCCCCCCFF, 0, 4, 0, false, 25}});
    rules.push_back({"aeroway", "taxiway",       {0xCCCCCCFF, 0, 2, 0, false, 25}});

    // ============================================================
    // z_order 26: Boundaries (topmost)
    // ============================================================
    rules.push_back({"boundary", "administrative", {0x9C429CFF, 0, 1, 0, false, 26, 10, 6}});
}

std::optional<Style> StyleEngine::style_for_way(
    const std::unordered_map<std::string, std::string>& tags) const
{
    // Boundaries: only render admin_level 2-6
    auto admin_it = tags.find("admin_level");
    bool is_boundary = false;
    if (admin_it != tags.end()) {
        int level = 0;
        auto [ptr, ec] = std::from_chars(admin_it->second.data(),
                                         admin_it->second.data() + admin_it->second.size(), level);
        if (ec == std::errc{} && level >= 2 && level <= 6) is_boundary = true;
    }

    for (const auto& rule : rules) {
        auto it = tags.find(rule.key);
        if (it == tags.end()) continue;
        if (rule.key == "boundary" && !is_boundary) continue;
        if (rule.value == "*" || it->second == rule.value) {
            return rule.style;
        }
    }
    return std::nullopt;
}
