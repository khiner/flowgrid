#include "UI.h"

using namespace ImGui;

float CalcAlignedX(const HAlign h_align, const float inner_w, const float outer_w, bool is_label) {
    return h_align == HAlign_Center || (is_label && inner_w < outer_w) ? (outer_w - inner_w) / 2 : h_align == HAlign_Left ? 0 : outer_w - inner_w;
}
float CalcAlignedY(const VAlign v_align, const float inner_h, const float outer_h) {
    return v_align == VAlign_Center ? (outer_h - inner_h) / 2 : v_align == VAlign_Top ? 0 : outer_h - inner_h;
}

ImVec2 TextSize(const string &text) { return CalcTextSize(text.c_str()); }

string Ellipsify(const string &text, const float max_width) {
    string ellipsified = text;
    while (TextSize(ellipsified).x > max_width && ellipsified.length() > 4) ellipsified.replace(ellipsified.end() - 4, ellipsified.end(), "...");
    return ellipsified;
}
