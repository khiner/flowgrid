#pragma once

#include <string>

#include "Fonts.h"

using u32 = unsigned int;

struct ImVec2;

enum Dir_ {
    Dir_None = -1,
    Dir_Left = 0,
    Dir_Right = 1,
    Dir_Up = 2,
    Dir_Down = 3,
    Dir_COUNT
};

// Uses same argument ordering as CSS.
struct Padding {
    const float Top, Right, Bottom, Left;

    Padding(float top, float right, float bottom, float left) : Top(top), Right(right), Bottom(bottom), Left(left) {}
    Padding(float top, float x, float bottom) : Padding(top, x, bottom, x) {}
    Padding(float y, float x) : Padding(y, x, y, x) {}
    Padding(float all) : Padding(all, all, all, all) {}
    Padding() : Padding(0, 0, 0, 0) {}
};

enum HJustify_ {
    HJustify_Left,
    HJustify_Middle,
    HJustify_Right,
};
enum VJustify_ {
    VJustify_Top,
    VJustify_Middle,
    VJustify_Bottom,
};
using HJustify = int;
using VJustify = int;

struct Justify {
    HJustify h;
    VJustify v;
};

float CalcAlignedX(HJustify h_justify, float inner_w, float outer_w, bool is_label = false); // todo better name than `is_label`
float CalcAlignedY(VJustify v_justify, float inner_h, float outer_h);

ImVec2 CalcTextSize(const std::string &);

// There's `RenderTextEllipsis` in `imgui_internal`, but it's way too complex and scary.
std::string Ellipsify(std::string copy, float max_width);

void FillRowItemBg(u32 color);
