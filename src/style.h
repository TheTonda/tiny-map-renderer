#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct Style {
    uint32_t color;
    uint32_t casing_color = 0;
    int width;
    int casing_width = 0;
    bool fill;
    int z_order;
    uint8_t dash_on = 0;   // 0 = solid line; >0 = dashed (pixels on)
    uint8_t dash_off = 0;  // pixels off between dashes
};

class StyleEngine {
public:
    StyleEngine();

    std::optional<Style> style_for_way(const std::unordered_map<std::string, std::string>& tags) const;

private:
    struct Rule {
        std::string key;
        std::string value;
        Style style;
    };

    std::vector<Rule> rules;
};
