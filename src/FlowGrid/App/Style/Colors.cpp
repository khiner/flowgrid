#include "Colors.h"

#include "UI/HelpMarker.h"

#include "imgui.h"

using namespace ImGui;

// Special color used to indicate that a color should be deduced automatically.
// Copied from `implot.h`.
#define IMPLOT_AUTO_COL ImVec4(0, 0, 0, -1)

Colors::Colors(ComponentArgs &&args, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto)
    : Component(std::move(args)), AllowAuto(allow_auto) {
    for (Count i = 0; i < size; i++) {
        new UInt({this, to_string(i), get_color_name(i)}); // Adds to `Children` as a side effect.
    }
}
Colors::~Colors() {
    const Count size = Size();
    for (int i = size - 1; i >= 0; i--) {
        delete Children[i];
    }
}

U32 Colors::ConvertFloat4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? UInt::AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
ImVec4 Colors::ConvertU32ToFloat4(const U32 value) { return value == UInt::AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }
Count Colors::Size() const { return Children.size(); }

const UInt *Colors::At(Count i) const { return static_cast<const UInt *>(Children[i]); }
U32 Colors::operator[](Count i) const { return *At(i); };
void Colors::Set(const std::vector<ImVec4> &values) const {
    for (Count i = 0; i < values.size(); i++) {
        At(i)->Set(ConvertFloat4ToU32(values[i]));
    }
}
void Colors::Set(const std::vector<std::pair<int, ImVec4>> &entries) const {
    for (const auto &[i, v] : entries) {
        At(i)->Set(ConvertFloat4ToU32(v));
    }
}

void Colors::Render() const {
    static ImGuiTextFilter filter;
    filter.Draw("Filter colors", GetFontSize() * 16);

    static ImGuiColorEditFlags flags = 0;
    if (RadioButton("Opaque", flags == ImGuiColorEditFlags_None)) flags = ImGuiColorEditFlags_None;
    SameLine();
    if (RadioButton("Alpha", flags == ImGuiColorEditFlags_AlphaPreview)) flags = ImGuiColorEditFlags_AlphaPreview;
    SameLine();
    if (RadioButton("Both", flags == ImGuiColorEditFlags_AlphaPreviewHalf)) flags = ImGuiColorEditFlags_AlphaPreviewHalf;
    SameLine();
    fg::HelpMarker("In the color list:\n"
                   "Left-click on color square to open color picker.\n"
                   "Right-click to open edit options menu.");

    BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
    PushItemWidth(-160);

    for (const auto *child : Children) {
        const auto *child_color = static_cast<const UInt *>(child);
        if (filter.PassFilter(child->Name.c_str())) {
            child_color->ColorEdit4(flags, AllowAuto);
        }
    }
    if (AllowAuto) {
        Separator();
        PushTextWrapPos(0);
        Text("Colors that are set to Auto will be automatically deduced from your ImGui style or the current ImPlot colormap.\n"
             "If you want to style individual plot items, use Push/PopStyleColor around its function.");
        PopTextWrapPos();
    }

    PopItemWidth();
    EndChild();
}
