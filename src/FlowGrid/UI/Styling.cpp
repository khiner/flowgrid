#include "Styling.h"

#include "imgui_internal.h"

using namespace ImGui;

float CalcAlignedX(const HJustify h_justify, const float inner_w, const float outer_w, bool is_label) {
    if (h_justify == HJustify_Middle || (is_label && inner_w < outer_w)) return (outer_w - inner_w) / 2;
    if (h_justify == HJustify_Left) return 0;
    return outer_w - inner_w;
}
float CalcAlignedY(const VJustify v_justify, const float inner_h, const float outer_h) {
    if (v_justify == VJustify_Middle) return (outer_h - inner_h) / 2;
    if (v_justify == VJustify_Top) return 0;
    return outer_h - inner_h;
}

ImVec2 CalcTextSize(const string &text) { return CalcTextSize(text.c_str()); }

// todo very inefficient
string Ellipsify(string copy, const float max_width) {
    while (CalcTextSize(copy).x > max_width && copy.length() > 4) copy.replace(copy.end() - 4, copy.end(), "...");
    return copy;
}
