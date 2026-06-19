#include "style.h"
#include <cstdlib>
#include <iostream>
#include <unordered_map>

static int failures = 0;

static void check(bool condition, const char* desc) {
    if (condition) {
        std::cout << "PASS: " << desc << '\n';
    } else {
        std::cout << "FAIL: " << desc << '\n';
        ++failures;
    }
}

int main() {
    StyleEngine engine;

    {
        std::unordered_map<std::string, std::string> tags = {{"highway", "primary"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=primary -> has style");
        check(style->color == 0xF9B29CFF, "highway=primary -> color=0xF9B29CFF");
        check(style->width == 4, "highway=primary -> width=4");
        check(style->fill == false, "highway=primary -> fill=false");
        check(style->z_order == 8, "highway=primary -> z_order=8");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"building", "yes"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "building=yes -> has style");
        check(style->fill == true, "building=yes -> fill=true");
        check(style->width == 0, "building=yes -> width=0");
        check(style->z_order == 2, "building=yes -> z_order=2");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"building", "church"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "building=church -> has style (wildcard)");
        check(style->fill == true, "building=church -> fill=true (wildcard)");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"highway", "service"}};
        auto style = engine.style_for_way(tags);
        check(!style.has_value(), "highway=service -> nullopt (not in rules)");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"name", "Some Street"}};
        auto style = engine.style_for_way(tags);
        check(!style.has_value(), "name=Some Street -> nullopt (no matching key)");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"waterway", "stream"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "waterway=stream -> has style");
        check(style->color == 0x7FC5E7FF, "waterway=stream -> color=0x7FC5E7FF");
        check(style->width == 2, "waterway=stream -> width=2");
    }

    {
        std::unordered_map<std::string, std::string> tags = {
            {"highway", "residential"},
            {"name", "Foo St"}
        };
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=residential + name -> has style");
        check(style->width == 2, "highway=residential + name -> width=2 (first match wins)");
    }

    if (failures == 0) {
        std::cout << "\nAll tests passed.\n";
    } else {
        std::cout << '\n' << failures << " test(s) failed.\n";
    }
    return failures == 0 ? 0 : 1;
}
