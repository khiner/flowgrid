#include "FaustParam.h"

#include "Core/Store/Store.h"
#include "FaustParamsStyle.h"
#include "UI/Widgets.h"

#include <range/v3/numeric/accumulate.hpp>

#include <imgui.h>

using namespace ImGui;
using namespace fg;

using enum FaustParamType;
using std::min, std::max;

FaustParam::FaustParam(ComponentArgs &&args, const FaustParamsStyle &style, const FaustParamType type, std::string_view label, Real *zone, Real min, Real max, Real init, Real step, const char *tooltip, NamesAndValues names_and_values)
    : FaustParamBase(style, type, label), Float(std::move(args), *zone), Zone(zone), Min(min), Max(max), Init(init), Step(step), Tooltip(tooltip), names_and_values(std::move(names_and_values)) {}

// todo config to place labels above horizontal params
float FaustParam::CalcWidth(bool include_label) const {
    const auto &imgui_style = ImGui::GetStyle();
    const bool has_label = include_label && !Label.empty();
    const float frame_height = GetFrameHeight();
    const float inner_spacing = imgui_style.ItemInnerSpacing.x;
    const float raw_label_width = CalcTextSize(Label.c_str()).x;
    const float label_width = has_label ? raw_label_width : 0;
    const float label_width_with_spacing = has_label ? raw_label_width + inner_spacing : 0;

    switch (Type) {
        case Type_NumEntry:
        case Type_HSlider:
        case Type_HBargraph: return Style.MinHorizontalItemWidth * frame_height + label_width_with_spacing;
        case Type_HRadioButtons: {
            return label_width_with_spacing +
                ranges::accumulate(names_and_values.names | std::views::transform(CalcRadioChoiceWidth), 0.f) +
                inner_spacing * float(names_and_values.Size());
        }
        case Type_Menu: {
            return label_width_with_spacing +
                std::ranges::max(
                       names_and_values.names |
                       std::views::transform([](const string &choice_name) { return CalcTextSize(choice_name).x; })
                ) +
                imgui_style.FramePadding.x * 2 + frame_height; // Extra frame for button
        }
        case Type_CheckButton: return frame_height + label_width_with_spacing;
        case Type_VBargraph:
        case Type_VSlider: return max(frame_height, label_width);
        case Type_VRadioButtons: return max(std::ranges::max(names_and_values.names | std::views::transform(CalcRadioChoiceWidth)), label_width);
        case Type_Button: return raw_label_width + imgui_style.FramePadding.x * 2; // Button uses label width even if `include_label == false`.
        case Type_Knob: return max(Style.MinKnobItemSize * frame_height, label_width);
        default: return FaustParamBase::CalcWidth(include_label);
    }
}

void FaustParam::Refresh() {
    Float::Refresh();
    *Zone = Value;
}

void FaustParam::Render(float suggested_height, bool no_label) const {
    const char *label = no_label ? "" : Label.c_str();
    const Justify justify = {Style.AlignmentHorizontal, Style.AlignmentVertical};
    const float frame_height = GetFrameHeight();
    const bool has_label = strlen(label) > 0;
    const float label_height = has_label ? CalcLabelHeight() : 0;
    const float available_x = GetContentRegionAvail().x;
    ImVec2 item_size_no_label = {CalcWidth(false), CalcHeight()};
    ImVec2 item_size = {has_label ? CalcWidth(true) : item_size_no_label.x, item_size_no_label.y + label_height};

    if (IsWidthExpandable() && available_x > item_size.x) {
        const float expand_delta_max = available_x - item_size.x;
        const float item_width_no_label_before = item_size_no_label.x;
        item_size_no_label.x = min(Style.MaxHorizontalItemWidth * frame_height, item_size_no_label.x + expand_delta_max);
        item_size.x += item_size_no_label.x - item_width_no_label_before;
    }
    if (IsHeightExpandable() && suggested_height > item_size.y) item_size.y = suggested_height;
    SetNextItemWidth(item_size_no_label.x);

    const auto old_cursor = GetCursorPos();
    SetCursorPos(old_cursor + ImVec2{max(0.f, CalcAlignedX(justify.h, has_label && IsLabelSameLine() ? item_size.x : item_size_no_label.x, available_x)), max(0.f, CalcAlignedY(justify.v, item_size.y, max(item_size.y, suggested_height)))});

    if (Type == Type_Button) {
        Button(label);
        if (IsItemActivated() && *Zone == 0.0) IssueSet(1.0);
        else if (IsItemDeactivated() && *Zone == 1.0) IssueSet(0.0);
    } else if (Type == Type_CheckButton) {
        auto value = bool(*Zone);
        if (Checkbox(label, &value)) IssueSet(Real(value));
    } else if (Type == Type_NumEntry) {
        auto value = int(*Zone);
        if (InputInt(label, &value, int(Step))) IssueSet(std::clamp(Real(value), Min, Max));
    } else if (Type == Type_HSlider || Type == Type_VSlider || Type == Type_HBargraph || Type == Type_VBargraph) {
        auto value = float(*Zone);
        ValueBarFlags flags = ValueBarFlags_None;
        if (Type == Type_HBargraph || Type == Type_VBargraph) flags |= ValueBarFlags_ReadOnly;
        if (Type == Type_VBargraph || Type == Type_VSlider) flags |= ValueBarFlags_Vertical;
        if (!has_label) flags |= ValueBarFlags_NoTitle;
        if (ValueBar(Label.c_str(), &value, item_size.y - label_height, float(Min), float(Max), flags, justify.h)) IssueSet(Real(value));
    } else if (Type == Type_Knob) {
        auto value = float(*Zone);
        KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
        const int steps = Step == 0 ? 0 : int((Max - Min) / Step);
        if (Knob(Label.c_str(), &value, float(Min), float(Max), 0, nullptr, justify.h, steps == 0 || steps > 10 ? KnobType_WiperDot : KnobType_Stepped, flags, steps)) {
            IssueSet(Real(value));
        }
    } else if (Type == Type_HRadioButtons || Type == Type_VRadioButtons) {
        auto value = float(*Zone);
        RadioButtonsFlags flags = has_label ? RadioButtonsFlags_None : RadioButtonsFlags_NoTitle;
        if (Type == Type_VRadioButtons) flags |= ValueBarFlags_Vertical;
        SetNextItemWidth(item_size.x); // Include label in param width for radio buttons (inconsistent but just makes things easier).
        if (RadioButtons(Label.c_str(), &value, names_and_values, flags, justify)) IssueSet(Real(value));
    } else if (Type == Type_Menu) {
        auto value = float(*Zone);
        // todo handle not present
        const auto selected_index = find(names_and_values.values.begin(), names_and_values.values.end(), value) - names_and_values.values.begin();
        if (BeginCombo(Label.c_str(), names_and_values.names[selected_index].c_str())) {
            for (int i = 0; i < int(names_and_values.names.size()); i++) {
                const Real choice_value = names_and_values.values[i];
                const bool is_selected = value == choice_value;
                if (Selectable(names_and_values.names[i].c_str(), is_selected)) IssueSet(choice_value);
            }
            EndCombo();
        }
    }
    UpdateGesturing();

    if (Tooltip && IsItemHovered()) {
        // todo only leaf params, so group tooltips don't work.
        // todo hook up to Info pane hover info.
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(Tooltip);
        EndTooltip();
    }
}
