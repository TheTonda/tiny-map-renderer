#include "clip.h"

static const int LEFT   = 1;
static const int RIGHT  = 2;
static const int BOTTOM = 4;
static const int TOP    = 8;

static int compute_outcode(double x, double y, double xmin, double ymin, double xmax, double ymax) {
    int code = 0;
    if (x < xmin)       code |= LEFT;
    else if (x > xmax)  code |= RIGHT;
    if (y < ymin)       code |= TOP;
    else if (y > ymax)  code |= BOTTOM;
    return code;
}

bool clip_line_cohen_sutherland(double& x0, double& y0, double& x1, double& y1, const Rect& clip) {
    double xmin = clip.x;
    double ymin = clip.y;
    double xmax = clip.x + clip.w;
    double ymax = clip.y + clip.h;

    int out0 = compute_outcode(x0, y0, xmin, ymin, xmax, ymax);
    int out1 = compute_outcode(x1, y1, xmin, ymin, xmax, ymax);

    while (true) {
        if (!(out0 | out1)) return true;
        if (out0 & out1)    return false;

        int outcode_out = out0 ? out0 : out1;
        double x, y;

        if (outcode_out & TOP) {
            x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0);
            y = ymin;
        } else if (outcode_out & BOTTOM) {
            x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
            y = ymax;
        } else if (outcode_out & RIGHT) {
            y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
            x = xmax;
        } else {
            y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0);
            x = xmin;
        }

        if (outcode_out == out0) {
            x0 = x; y0 = y;
            out0 = compute_outcode(x0, y0, xmin, ymin, xmax, ymax);
        } else {
            x1 = x; y1 = y;
            out1 = compute_outcode(x1, y1, xmin, ymin, xmax, ymax);
        }
    }
}

static int compute_outcode_int(int x, int y, int xmin, int ymin, int xmax, int ymax) {
    int code = 0;
    if (x < xmin)       code |= LEFT;
    else if (x > xmax)  code |= RIGHT;
    if (y < ymin)       code |= TOP;
    else if (y > ymax)  code |= BOTTOM;
    return code;
}

bool clip_line_cohen_sutherland_int(int& x0, int& y0, int& x1, int& y1, int xmin, int ymin, int xmax, int ymax) {
    int out0 = compute_outcode_int(x0, y0, xmin, ymin, xmax, ymax);
    int out1 = compute_outcode_int(x1, y1, xmin, ymin, xmax, ymax);

    while (true) {
        if (!(out0 | out1)) return true;
        if (out0 & out1)    return false;

        int outcode_out = out0 ? out0 : out1;
        int x, y;

        if (outcode_out & TOP) {
            x = static_cast<int>(x0 + (x1 - x0) * static_cast<double>(ymin - y0) / (y1 - y0));
            y = ymin;
        } else if (outcode_out & BOTTOM) {
            x = static_cast<int>(x0 + (x1 - x0) * static_cast<double>(ymax - y0) / (y1 - y0));
            y = ymax;
        } else if (outcode_out & RIGHT) {
            y = static_cast<int>(y0 + (y1 - y0) * static_cast<double>(xmax - x0) / (x1 - x0));
            x = xmax;
        } else {
            y = static_cast<int>(y0 + (y1 - y0) * static_cast<double>(xmin - x0) / (x1 - x0));
            x = xmin;
        }

        if (outcode_out == out0) {
            x0 = x; y0 = y;
            out0 = compute_outcode_int(x0, y0, xmin, ymin, xmax, ymax);
        } else {
            x1 = x; y1 = y;
            out1 = compute_outcode_int(x1, y1, xmin, ymin, xmax, ymax);
        }
    }
}
