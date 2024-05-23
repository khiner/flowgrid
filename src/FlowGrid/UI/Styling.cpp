#include "Styling.h"

#include "imgui_internal.h"

using std::string, std::string_view;
using namespace ImGui;

float CalcAlignedX(HJustify h_justify, float inner_w, float outer_w, bool is_label) {
    if (h_justify == HJustify_Middle || (is_label && inner_w < outer_w)) return (outer_w - inner_w) / 2;
    if (h_justify == HJustify_Left) return 0;
    return outer_w - inner_w;
}
float CalcAlignedY(VJustify v_justify, float inner_h, float outer_h) {
    if (v_justify == VJustify_Middle) return (outer_h - inner_h) / 2;
    if (v_justify == VJustify_Top) return 0;
    return outer_h - inner_h;
}

ImVec2 CalcTextSize(string_view text) { return CalcTextSize(text.data()); }

// todo very inefficient
void Ellipsify(string &str, float max_width) {
    while (CalcTextSize(str).x > max_width && str.length() > 4) str.replace(str.end() - 4, str.end(), "...");
}

void FillRowItemBg(u32 color) {
    const ImVec2 row_min{GetWindowPos().x, GetCursorScreenPos().y};
    GetWindowDrawList()->AddRectFilled(row_min, row_min + ImVec2{GetWindowWidth(), GetFontSize()}, color);
}
