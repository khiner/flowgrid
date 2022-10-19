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

static float CalcAlignedX(const HAlign h_align, const float inner_w, const float outer_w, bool is_label = false) { // todo better name than `is_label`
    return h_align == HAlign_Center || (is_label && inner_w < outer_w) ? (outer_w - inner_w) / 2 : h_align == HAlign_Left ? 0 : outer_w - inner_w;
}
static float CalcAlignedY(const VAlign v_align, const float inner_h, const float outer_h) {
    return v_align == VAlign_Center ? (outer_h - inner_h) / 2 : v_align == VAlign_Top ? 0 : outer_h - inner_h;
}
