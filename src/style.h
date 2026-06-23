#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct Style {
    uint32_t color;
    uint32_t casing_color = 0;  // 0 = no casing
    int width;
    int casing_width = 0;       // extra width behind the line (draw casing first)
    bool fill;
    int z_order;
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
