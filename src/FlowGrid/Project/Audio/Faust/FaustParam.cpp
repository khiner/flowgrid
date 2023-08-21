#include "FaustParam.h"

#include "FaustParamsUIStyle.h"
#include "UI/Widgets.h"

#include <range/v3/numeric/accumulate.hpp>

#include <imgui.h>

using namespace ImGui;
using namespace fg;

using enum FaustParamType;
using std::min, std::max;

FaustParam::FaustParam(const FaustParamsUIStyle &style, const FaustParamType type, std::string_view label, Real *zone, Real min, Real max, Real init, Real step, const char *tooltip, NamesAndValues names_and_values)
    : Style(style), Type(type), Id(label), Label(label == "0x00" ? "" : label), Zone(zone), Min(min), Max(max), Init(init), Step(step), Tooltip(tooltip), names_and_values(std::move(names_and_values)) {}

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
        default: return GetContentRegionAvail().x;
    }
}

float FaustParam::CalcHeight() const {
    const float frame_height = GetFrameHeight();
    switch (Type) {
        case Type_VBargraph:
        case Type_VSlider:
        case Type_VRadioButtons: return Style.MinVerticalItemHeight * frame_height;
        case Type_HSlider:
        case Type_NumEntry:
        case Type_HBargraph:
        case Type_Button:
        case Type_CheckButton:
        case Type_HRadioButtons:
        case Type_Menu: return frame_height;
        case Type_Knob: return Style.MinKnobItemSize * frame_height + frame_height + ImGui::GetStyle().ItemSpacing.y;
        default: return 0;
    }
}

// Returns _additional_ height needed to accommodate a label for the param
float FaustParam::CalcLabelHeight() const {
    switch (Type) {
        case Type_VBargraph:
        case Type_VSlider:
        case Type_VRadioButtons:
        case Type_Knob:
        case Type_HGroup:
        case Type_VGroup:
        case Type_TGroup: return GetTextLineHeightWithSpacing();
        case Type_Button:
        case Type_HSlider:
        case Type_NumEntry:
        case Type_HBargraph:
        case Type_CheckButton:
        case Type_HRadioButtons:
        case Type_Menu:
        case Type_None: return 0;
    }
}

/**
* `suggested_height == 0` means no height suggestion.
* For params (as opposed to groups), the suggested height is the expected _available_ height in the group
  (which is relevant for aligning params relative to other params in the same group).
* Items/groups are allowed to extend beyond this height to fit its contents, if necessary.
* The cursor position is expected to be set appropriately below the drawn contents.
*/
void FaustParam::Draw(float suggested_height, bool no_label) const {
    if (IsGroup()) DrawGroup(suggested_height, no_label);
    else DrawParam(suggested_height, no_label);

    if (Tooltip && IsItemHovered()) {
        // todo only leaf params, so group tooltips don't work.
        // todo hook up to Info pane hover info.
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(Tooltip);
        EndTooltip();
    }
}

void FaustParam::DrawGroup(float suggested_height, bool no_label) const {
    if (!IsGroup()) return;

    const char *label = no_label ? "" : Label.c_str();
    const auto &imgui_style = ImGui::GetStyle();
    const auto &children = Children;
    const float frame_height = GetFrameHeight();
    const bool has_label = strlen(label) > 0;
    const float label_height = has_label ? CalcLabelHeight() : 0;

    if (has_label) TextUnformatted(label);

    if (Type == Type_TGroup) {
        const bool is_height_constrained = suggested_height != 0;
        // In addition to the group contents, account for the tab height and the space between the tabs and the content.
        const float group_height = max(0.f, is_height_constrained ? suggested_height - label_height : 0);
        const float item_height = max(0.f, group_height - frame_height - imgui_style.ItemSpacing.y);
        BeginTabBar(Label.c_str());
        for (const auto &child : children) {
            if (BeginTabItem(child.Label.c_str())) {
                child.Draw(item_height, true);
                EndTabItem();
            }
        }
        EndTabBar();
        return;
    }

    const float cell_padding = Type == Type_None ? 0 : 2 * imgui_style.CellPadding.y;
    const bool is_h = Type == Type_HGroup;
    float suggested_item_height = 0; // Including any label height, not including cell padding
    if (is_h) {
        const bool include_labels = !Style.HeaderTitles;
        suggested_item_height = std::ranges::max(
            Children | std::views::transform([include_labels](const auto &child) {
                return child.CalcHeight() + (include_labels ? child.CalcLabelHeight() : 0);
            })
        );
    }
    if (Type == Type_None) { // Root group (treated as a vertical group but not as a table)
        for (const auto &child : children) child.Draw(suggested_item_height);
        return;
    }

    if (BeginTable(Id.c_str(), is_h ? int(children.size()) : 1, TableFlagsToImGui(Style.TableFlags))) {
        const float row_min_height = suggested_item_height + cell_padding;
        if (is_h) {
            ParamsWidthSizingPolicy policy = Style.WidthSizingPolicy;
            const bool allow_fixed_width_params =
                policy != ParamsWidthSizingPolicy_Balanced &&
                (policy == ParamsWidthSizingPolicy_StretchFlexibleOnly ||
                 (policy == ParamsWidthSizingPolicy_StretchToFill &&
                  std::ranges::any_of(Children, [](const auto &child) { return child.IsWidthExpandable(); })));
            for (const auto &child : children) {
                ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                if (allow_fixed_width_params && !child.IsWidthExpandable()) flags |= ImGuiTableColumnFlags_WidthFixed;
                TableSetupColumn(child.Label.c_str(), flags, child.CalcWidth(true));
            }
            if (Style.HeaderTitles) {
                // Custom headers (instead of `TableHeadersRow()`) to align column names.
                TableNextRow(ImGuiTableRowFlags_Headers);
                for (int column = 0; column < int(children.size()); column++) {
                    TableSetColumnIndex(column);
                    const char *column_name = TableGetColumnName(column);
                    PushID(column);
                    const float header_x = CalcAlignedX(Style.AlignmentHorizontal, CalcTextSize(column_name).x, GetContentRegionAvail().x);
                    SetCursorPosX(GetCursorPosX() + max(0.f, header_x));
                    TableHeader(column_name);
                    PopID();
                }
            }
            TableNextRow(ImGuiTableRowFlags_None, row_min_height);
        }
        for (const auto &child : children) {
            if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_min_height);
            TableNextColumn();
            TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_TitleBgActive, 0.1f));
            const string child_label = child.Type == Type_Button || !is_h || !Style.HeaderTitles ? child.Label : "";
            child.Draw(suggested_item_height);
        }
        EndTable();
    }
}

void FaustParam::DrawParam(float suggested_height, bool no_label) const {
    if (IsGroup()) return;

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

    Real *zone = Zone;
    if (Type == Type_Button) {
        Button(label);
        if (IsItemActivated() && *zone == 0.0) *zone = 1.0;
        else if (IsItemDeactivated() && *zone == 1.0) *zone = 0.0;
    } else if (Type == Type_CheckButton) {
        auto value = bool(*zone);
        if (Checkbox(label, &value)) *zone = Real(value);
    } else if (Type == Type_NumEntry) {
        auto value = int(*zone);
        if (InputInt(label, &value, int(Step))) *zone = std::clamp(Real(value), Min, Max);
    } else if (Type == Type_HSlider || Type == Type_VSlider || Type == Type_HBargraph || Type == Type_VBargraph) {
        auto value = float(*zone);
        ValueBarFlags flags = ValueBarFlags_None;
        if (Type == Type_HBargraph || Type == Type_VBargraph) flags |= ValueBarFlags_ReadOnly;
        if (Type == Type_VBargraph || Type == Type_VSlider) flags |= ValueBarFlags_Vertical;
        if (!has_label) flags |= ValueBarFlags_NoTitle;
        if (ValueBar(Label.c_str(), &value, item_size.y - label_height, float(Min), float(Max), flags, justify.h)) *zone = Real(value);
    } else if (Type == Type_Knob) {
        auto value = float(*zone);
        KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
        const int steps = Step == 0 ? 0 : int((Max - Min) / Step);
        if (Knob(Label.c_str(), &value, float(Min), float(Max), 0, nullptr, justify.h, steps == 0 || steps > 10 ? KnobType_WiperDot : KnobType_Stepped, flags, steps)) {
            *zone = Real(value);
        }
    } else if (Type == Type_HRadioButtons || Type == Type_VRadioButtons) {
        auto value = float(*zone);
        RadioButtonsFlags flags = has_label ? RadioButtonsFlags_None : RadioButtonsFlags_NoTitle;
        if (Type == Type_VRadioButtons) flags |= ValueBarFlags_Vertical;
        SetNextItemWidth(item_size.x); // Include label in param width for radio buttons (inconsistent but just makes things easier).
        if (RadioButtons(Label.c_str(), &value, names_and_values, flags, justify)) *zone = Real(value);
    } else if (Type == Type_Menu) {
        auto value = float(*zone);
        // todo handle not present
        const auto selected_index = find(names_and_values.values.begin(), names_and_values.values.end(), value) - names_and_values.values.begin();
        if (BeginCombo(Label.c_str(), names_and_values.names[selected_index].c_str())) {
            for (int i = 0; i < int(names_and_values.names.size()); i++) {
                const Real choice_value = names_and_values.values[i];
                const bool is_selected = value == choice_value;
                if (Selectable(names_and_values.names[i].c_str(), is_selected)) *zone = Real(choice_value);
            }
            EndCombo();
        }
    }
}
