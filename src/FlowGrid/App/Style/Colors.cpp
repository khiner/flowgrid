#include "Colors.h"

#include <range/v3/range/conversion.hpp>

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Widgets.h"

using namespace ImGui;

Colors::Colors(ComponentArgs &&args, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto)
    : Vector<U32>(std::move(args)), GetColorName(get_color_name), AllowAuto(allow_auto) {
    Vector<U32>::Set(std::views::iota(0, int(size)) | ranges::to<std::vector<U32>>);
}

U32 Colors::Float4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
ImVec4 Colors::U32ToFloat4(U32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

void Colors::Set(const std::vector<ImVec4> &values) const {
    Vector<U32>::Set(values | std::views::transform([](const auto &value) { return Float4ToU32(value); }) | ranges::to<std::vector>);
}
void Colors::Set(const std::vector<std::pair<int, ImVec4>> &entries) const {
    Vector<U32>::Set(entries | std::views::transform([](const auto &entry) { return std::pair(entry.first, Float4ToU32(entry.second)); }) | ranges::to<std::vector>);
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

    for (Count i = 0; i < Size(); i++) {
        const std::string &color_name = GetColorName(i);
        if (filter.PassFilter(color_name.c_str())) {
            U32 color = Value[i];
            const bool is_auto = AllowAuto && color == AutoColor;
            const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : color;

            PushID(i);
            fg::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
            SetItemAllowOverlap();

            // todo use auto for FG colors (link to ImGui colors)
            if (AllowAuto) {
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
                if (Button("Auto")) Action::Vector<U32>::SetAt{Path, i, is_auto ? mapped_value : AutoColor}.q();
                if (!is_auto) PopStyleVar();
                SameLine();
            }

            auto value = ColorConvertU32ToFloat4(mapped_value);
            if (is_auto) BeginDisabled();
            const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (AllowAuto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            UpdateGesturing();
            if (is_auto) EndDisabled();

            SameLine(0, GetStyle().ItemInnerSpacing.x);
            TextUnformatted(color_name.c_str());

            PopID();

            if (changed) Action::Vector<U32>::SetAt{Path, i, ColorConvertFloat4ToU32(value)}.q();
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

void Colors::RenderValueTree(ValueTreeLabelMode mode, bool auto_select) const {
    Field::RenderValueTree(mode, auto_select);

    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (fg::TreeNode(Name)) {
        for (Count i = 0; i < Value.size(); i++) {
            const std::string &label = mode == Annotated ? GetColorName(i) : to_string(i);
            TreeNodeFlags flags = TreeNodeFlags_None;
            if (mode == Annotated) flags |= TreeNodeFlags_Highlighted;
            fg::TreeNode(label, flags, nullptr, U32ToHex(Value[i]).c_str());
        }
        TreePop();
    }
}
