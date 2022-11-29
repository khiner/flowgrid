#pragma once

#include <string>

#include "imgui.h"

using std::string;

// Uses same argument ordering as CSS.
struct Padding {
    Padding(const float top, const float right, const float bottom, const float left) : Top(top), Right(right), Bottom(bottom), Left(left) {}
    Padding(const float top, const float x, const float bottom) : Padding(top, x, bottom, x) {}
    Padding(const float y, const float x) : Padding(y, x, y, x) {}
    Padding(const float all) : Padding(all, all, all, all) {}
    Padding() : Padding(0, 0, 0, 0) {}

    const float Top, Right, Bottom, Left;
};

struct TextStyle {
    enum Justify { Left, Middle, Right, };
    enum FontStyle { Normal, Bold, Italic, };

    const ImColor Color;
    const Justify Justify{Middle};
    const Padding Padding;
    const FontStyle FontStyle{Normal};
};

struct RectStyle {
    const ImColor FillColor, StrokeColor;
    const float StrokeWidth{0}, CornerRadius{0};
};

enum HAlign_ { HAlign_Left, HAlign_Center, HAlign_Right, };
enum VAlign_ { VAlign_Top, VAlign_Center, VAlign_Bottom, };
using HAlign = int;
using VAlign = int;

struct Align {
    HAlign x;
    VAlign y;
};

float CalcAlignedX(HAlign h_align, float inner_w, float outer_w, bool is_label = false); // todo better name than `is_label`
float CalcAlignedY(VAlign v_align, float inner_h, float outer_h);

ImVec2 TextSize(const string &text);

// There's `RenderTextEllipsis` in `imgui_internal`, but it's way too complex and scary.
string Ellipsify(const string &text, float max_width);
