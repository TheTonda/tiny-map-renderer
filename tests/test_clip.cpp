#include "clip.h"
#include <cstdio>

static int failures = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        std::printf("PASS: %s\n", name);
    } else {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

int main() {
    Rect r = {0, 0, 100, 100};

    {
        double x0 = 25, y0 = 25, x1 = 75, y1 = 75;
        bool result = clip_line_cohen_sutherland(x0, y0, x1, y1, r);
        check("fully inside: returns true", result);
        check("fully inside: x0 unchanged", x0 == 25);
        check("fully inside: y0 unchanged", y0 == 25);
        check("fully inside: x1 unchanged", x1 == 75);
        check("fully inside: y1 unchanged", y1 == 75);
    }

    {
        double x0 = 150, y0 = 150, x1 = 200, y1 = 200;
        bool result = clip_line_cohen_sutherland(x0, y0, x1, y1, r);
        check("fully outside: returns false", !result);
    }

    {
        double x0 = -50, y0 = 50, x1 = 50, y1 = 50;
        bool result = clip_line_cohen_sutherland(x0, y0, x1, y1, r);
        check("cross left edge: returns true", result);
        check("cross left edge: x0 clipped to 0", x0 == 0);
        check("cross left edge: x1 unchanged", x1 == 50);
    }

    {
        double x0 = -50, y0 = -50, x1 = 150, y1 = 150;
        bool result = clip_line_cohen_sutherland(x0, y0, x1, y1, r);
        check("cross corner: returns true", result);
        check("cross corner: x0 clipped to 0", x0 == 0);
        check("cross corner: y0 clipped to 0", y0 == 0);
        check("cross corner: x1 clipped to 100", x1 == 100);
        check("cross corner: y1 clipped to 100", y1 == 100);
    }

    {
        double x0 = 50, y0 = -50, x1 = 50, y1 = 150;
        bool result = clip_line_cohen_sutherland(x0, y0, x1, y1, r);
        check("vertical partial: returns true", result);
        check("vertical partial: y0 clipped to 0", y0 == 0);
        check("vertical partial: y1 clipped to 100", y1 == 100);
        check("vertical partial: x unchanged", x0 == 50 && x1 == 50);
    }

    {
        int x0 = -10, y0 = 50, x1 = 110, y1 = 50;
        bool result = clip_line_cohen_sutherland_int(x0, y0, x1, y1, 0, 0, 100, 100);
        check("int clip: returns true", result);
        check("int clip: x0 clipped to 0", x0 == 0);
        check("int clip: x1 clipped to 100", x1 == 100);
        check("int clip: y unchanged", y0 == 50 && y1 == 50);
    }

    return failures > 0 ? 1 : 0;
}
