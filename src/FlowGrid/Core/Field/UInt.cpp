#include "UInt.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "UI/InvisibleButton.h"

UInt::UInt(Stateful *parent, string_view path_segment, string_view name_help, U32 value, U32 min, U32 max)
    : TypedField(parent, path_segment, name_help, value), Min(min), Max(max) {}
UInt::UInt(Stateful *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value)
    : TypedField(parent, path_segment, name_help, value), Min(0), Max(100), GetName(std::move(get_name)) {}
UInt::operator bool() const { return Value; }
UInt::operator int() const { return Value; }
UInt::operator ImColor() const { return Value; }
string UInt::ValueName(const U32 value) const { return GetName ? (*GetName)(value) : to_string(value); }

using namespace ImGui;

void UInt::Render() const {
    U32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UpdateGesturing();
    if (edited) Action::SetValue{Path, value}.q();
    HelpMarker();
}
void UInt::Render(const std::vector<U32> &options) const {
    if (options.empty()) return;

    const U32 value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) Action::SetValue{Path, option}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void UInt::ColorEdit4(ImGuiColorEditFlags flags, bool allow_auto) const {
    const Count i = std::stoi(PathSegment); // Assuming color is a member of a vector here.
    const bool is_auto = allow_auto && Value == AutoColor;
    const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : Value;

    PushID(ImGuiLabel.c_str());
    fg::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
    SetItemAllowOverlap();

    // todo use auto for FG colors (link to ImGui colors)
    if (allow_auto) {
        if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
        if (Button("Auto")) Action::SetValue{Path, is_auto ? mapped_value : AutoColor}.q();
        if (!is_auto) PopStyleVar();
        SameLine();
    }

    auto value = ColorConvertU32ToFloat4(mapped_value);
    if (is_auto) BeginDisabled();
    const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (allow_auto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
    UpdateGesturing();
    if (is_auto) EndDisabled();

    SameLine(0, GetStyle().ItemInnerSpacing.x);
    TextUnformatted(Name.c_str());

    PopID();

    if (changed) Action::SetValue{Path, ColorConvertFloat4ToU32(value)}.q();
}
