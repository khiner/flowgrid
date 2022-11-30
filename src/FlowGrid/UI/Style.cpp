#include "Style.h"

using namespace ImGui;

float CalcAlignedX(const HJustify h_justify, const float inner_w, const float outer_w, bool is_label) {
    return h_justify == HJustify_Middle || (is_label && inner_w < outer_w) ? (outer_w - inner_w) / 2 : h_justify == HJustify_Left ? 0 : outer_w - inner_w;
}
float CalcAlignedY(const VJustify v_justify, const float inner_h, const float outer_h) {
    return v_justify == VJustify_Middle ? (outer_h - inner_h) / 2 : v_justify == VJustify_Top ? 0 : outer_h - inner_h;
}

ImVec2 TextSize(const string &text) { return CalcTextSize(text.c_str()); }

string Ellipsify(const string &text, const float max_width) {
    string ellipsified = text;
    while (TextSize(ellipsified).x > max_width && ellipsified.length() > 4) ellipsified.replace(ellipsified.end() - 4, ellipsified.end(), "...");
    return ellipsified;
}
