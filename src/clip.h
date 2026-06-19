#pragma once

struct Rect {
    double x, y, w, h;
};

bool clip_line_cohen_sutherland(double& x0, double& y0, double& x1, double& y1, const Rect& clip);
bool clip_line_cohen_sutherland_int(int& x0, int& y0, int& x1, int& y1, int xmin, int ymin, int xmax, int ymax);
