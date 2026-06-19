#include "style.h"

StyleEngine::StyleEngine() {
    rules.push_back({"highway", "motorway",    {0xE892A2FF, 6, false, 10}});
    rules.push_back({"highway", "trunk",       {0xFCD6A4FF, 5, false, 9}});
    rules.push_back({"highway", "primary",     {0xF9B29CFF, 4, false, 8}});
    rules.push_back({"highway", "secondary",   {0xFDBF6FFF, 3, false, 7}});
    rules.push_back({"highway", "residential", {0xBBBBBBFF, 2, false, 5}});
    rules.push_back({"highway", "footway",     {0x999999FF, 1, false, 3}});
    rules.push_back({"building", "*",          {0xD9D0C9FF, 0, true,  2}});
    rules.push_back({"landuse", "forest",      {0xAEDCA3FF, 0, true,  1}});
    rules.push_back({"waterway", "*",          {0x7FC5E7FF, 2, false, 4}});
    rules.push_back({"natural", "water",       {0x7FC5E7FF, 0, true,  1}});
}

std::optional<Style> StyleEngine::style_for_way(
    const std::unordered_map<std::string, std::string>& tags) const
{
    for (const auto& rule : rules) {
        auto it = tags.find(rule.key);
        if (it == tags.end()) {
            continue;
        }
        if (rule.value == "*" || it->second == rule.value) {
            return rule.style;
        }
    }
    return std::nullopt;
}
