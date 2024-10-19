#include "Colors.h"

#include <ranges>

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "Helper/Hex.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"

using namespace ImGui;

using std::views::transform, std::ranges::to;

Colors::Colors(ArgsT &&args, u32 size, std::function<const char *(int)> get_name, const bool allow_auto)
    : Vector(std::move(args.Args)), ActionProducer(std::move(args.Q)), GetName(get_name), AllowAuto(allow_auto) {
    Vector::Set(std::views::iota(0u, u32(size)) | to<std::vector>());
}

u32 Colors::Float4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
ImVec4 Colors::U32ToFloat4(u32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

void Colors::Set(const std::vector<ImVec4> &values) const {
    Vector::Set(values | transform([](const auto &value) { return Float4ToU32(value); }) | to<std::vector>());
}
void Colors::Set(const std::unordered_map<size_t, ImVec4> &entries) const {
    Vector::Set(entries | transform([](const auto &entry) { return std::pair(entry.first, Float4ToU32(entry.second)); }) | to<std::unordered_map>());
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

    for (u32 i = 0; i < Size(); i++) {
        if (const std::string &color_name = GetName(i); filter.PassFilter(color_name.c_str())) {
            u32 color = (*this)[i];
            const bool is_auto = AllowAuto && color == AutoColor;
            const u32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : color;

            PushID(i);
            fg::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
            SetItemAllowOverlap();

            // todo use auto for FG colors (link to ImGui colors)
            if (AllowAuto) {
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
                if (Button("Auto")) Q(Action::Vector<u32>::Set{Id, i, is_auto ? mapped_value : AutoColor});
                if (!is_auto) PopStyleVar();
                SameLine();
            }

            auto value = ColorConvertU32ToFloat4(mapped_value);
            if (is_auto) BeginDisabled();
            const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (AllowAuto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            UpdateGesturing();
            if (is_auto) EndDisabled();

            SameLine(0, GetStyle().ItemInnerSpacing.x);
            TextUnformatted(color_name);

            PopID();

            if (changed) Q(Action::Vector<u32>::Set{Id, i, ColorConvertFloat4ToU32(value)});
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

void Colors::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    if (TreeNode(Name)) {
        const auto &value = Get();
        for (u32 i = 0; i < value.size(); i++) {
            TreeNode(annotate ? GetName(i) : std::to_string(i), annotate, U32ToHex(value[i], true).c_str());
        }
        TreePop();
    }
}
