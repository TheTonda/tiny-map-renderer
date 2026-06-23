#include "style.h"
#include <cstdlib>
#include <cstdio>
#include <unordered_map>

static int failures = 0;

static void check(bool condition, const char* desc) {
    if (condition) {
        std::printf("PASS: %s\n", desc);
    } else {
        std::printf("FAIL: %s\n", desc);
        ++failures;
    }
}

int main() {
    StyleEngine engine;

    {
        std::unordered_map<std::string, std::string> tags = {{"highway", "primary"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=primary -> has style");
        check(style->color == 0xFDBF6FFF, "highway=primary -> color");
        check(style->casing_color == 0xD99B5EFF, "highway=primary -> casing_color");
        check(style->width == 5, "highway=primary -> width=5");
        check(style->casing_width == 2, "highway=primary -> casing_width=2");
        check(style->fill == false, "highway=primary -> fill=false");
        check(style->z_order == 17, "highway=primary -> z_order=17");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"building", "yes"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "building=yes -> has style");
        check(style->fill == true, "building=yes -> fill=true");
        check(style->width == 0, "building=yes -> width=0");
        check(style->z_order == 1, "building=yes -> z_order=1");
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
        check(style.has_value(), "highway=service -> has style (now in rules)");
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
        check(style->color == 0xB5D0D0FF, "waterway=stream -> color=0xB5D0D0FF");
        check(style->width == 1, "waterway=stream -> width=1");
    }

    {
        std::unordered_map<std::string, std::string> tags = {
            {"highway", "residential"},
            {"name", "Foo St"}
        };
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=residential + name -> has style");
        check(style->width == 2, "highway=residential + name -> width=2");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"highway", "motorway"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=motorway -> has style");
        check(style->casing_width == 3, "highway=motorway -> casing_width=3");
        check(style->casing_color == 0xC54E6EFF, "highway=motorway -> casing_color");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"highway", "cycleway"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "highway=cycleway -> has style");
        check(style->casing_width == 0, "highway=cycleway -> no casing");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"landuse", "forest"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "landuse=forest -> has style");
        check(style->fill == true, "landuse=forest -> fill=true");
        check(style->z_order == 0, "landuse=forest -> z_order=0 (background)");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"leisure", "park"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "leisure=park -> has style");
        check(style->fill == true, "leisure=park -> fill=true");
    }

    {
        std::unordered_map<std::string, std::string> tags = {{"railway", "rail"}};
        auto style = engine.style_for_way(tags);
        check(style.has_value(), "railway=rail -> has style");
        check(style->fill == false, "railway=rail -> fill=false");
    }

    if (failures == 0) {
        std::printf("\nAll tests passed.\n");
    } else {
        std::printf("\n%d test(s) failed.\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
