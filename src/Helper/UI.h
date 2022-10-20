#pragma once

enum HAlign_ {
    HAlign_Left,
    HAlign_Center,
    HAlign_Right,
};
enum VAlign_ {
    VAlign_Top,
    VAlign_Center,
    VAlign_Bottom,
};
using HAlign = int;
using VAlign = int;

struct ImVec2i {
    int x, y;
};
using Align = ImVec2i; // E.g. `{HAlign_Center, VAlign_Bottom}`

float CalcAlignedX(HAlign h_align, float inner_w, float outer_w, bool is_label = false); // todo better name than `is_label`
float CalcAlignedY(VAlign v_align, float inner_h, float outer_h);
