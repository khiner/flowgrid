#include "FaustParamsUI.h"

#include <range/v3/numeric/accumulate.hpp>

#include "App/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"

#include <imgui_internal.h>

#include "FaustParams.h"
#include "UI/Widgets.h"

using namespace ImGui;
using namespace fg;

using enum FaustParam::Type;
using std::min, std::max;

static std::unique_ptr<FaustParamsUI> Ui;

static bool IsWidthExpandable(const FaustParam::Type type) {
    return type == Type_HGroup || type == Type_VGroup || type == Type_TGroup || type == Type_NumEntry || type == Type_HSlider || type == Type_HBargraph;
}
static bool IsHeightExpandable(const FaustParam::Type type) {
    return type == Type_VBargraph || type == Type_VSlider || type == Type_CheckButton;
}
static bool IsLabelSameLine(const FaustParam::Type type) {
    return type == Type_NumEntry || type == Type_HSlider || type == Type_HBargraph || type == Type_HRadioButtons || type == Type_Menu || type == Type_CheckButton;
}

// todo config to place labels above horizontal params
float FaustParams::CalcWidth(const FaustParam &param, const bool include_label) const {
    const auto &imgui_style = ImGui::GetStyle();
    const bool has_label = include_label && !param.label.empty();
    const float frame_height = GetFrameHeight();
    const float inner_spacing = imgui_style.ItemInnerSpacing.x;
    const float raw_label_width = CalcTextSize(param.label.c_str()).x;
    const float label_width = has_label ? raw_label_width : 0;
    const float label_width_with_spacing = has_label ? raw_label_width + inner_spacing : 0;

    switch (param.type) {
        case Type_NumEntry:
        case Type_HSlider:
        case Type_HBargraph: return Style.MinHorizontalItemWidth * frame_height + label_width_with_spacing;
        case Type_HRadioButtons: {
            return label_width_with_spacing +
                ranges::accumulate(Ui->NamesAndValues[param.zone].names | std::views::transform(CalcRadioChoiceWidth), 0.f) +
                inner_spacing * float(Ui->NamesAndValues.size());
        }
        case Type_Menu: {
            return label_width_with_spacing + std::ranges::max(Ui->NamesAndValues[param.zone].names | std::views::transform([](const string &choice_name) {
                                                                   return CalcTextSize(choice_name).x;
                                                               })) +
                GetStyle().FramePadding.x * 2 + frame_height; // Extra frame for button
        }
        case Type_CheckButton: return frame_height + label_width_with_spacing;
        case Type_VBargraph:
        case Type_VSlider: return max(frame_height, label_width);
        case Type_VRadioButtons: return max(std::ranges::max(Ui->NamesAndValues[param.zone].names | std::views::transform(CalcRadioChoiceWidth)), label_width);
        case Type_Button: return raw_label_width + imgui_style.FramePadding.x * 2; // Button uses label width even if `include_label == false`.
        case Type_Knob: return max(Style.MinKnobItemSize * frame_height, label_width);
        default: return GetContentRegionAvail().x;
    }
}
float FaustParams::CalcHeight(const FaustParam &param) const {
    const float frame_height = GetFrameHeight();
    switch (param.type) {
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
float FaustParams::CalcLabelHeight(const FaustParam &param) const {
    switch (param.type) {
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

// `suggested_height` may be positive if the param is within a constrained layout setting.
// `suggested_height == 0` means no height suggestion.
// For _params_ (as opposed to groups), the suggested height is the expected _available_ height in the group
// (which is relevant for aligning params relative to other params in the same group).
// Items/groups are allowed to extend beyond this height if needed to fit its contents.
// It is expected that the cursor position will be set appropriately below the drawn contents.
void FaustParams::DrawUiItem(const FaustParam &param, const char *label, const float suggested_height) const {
    const auto &imgui_style = ImGui::GetStyle();
    const Justify justify = {Style.AlignmentHorizontal, Style.AlignmentVertical};
    const auto type = param.type;
    const auto &children = param.children;
    const float frame_height = GetFrameHeight();
    const bool has_label = strlen(label) > 0;
    const float label_height = has_label ? CalcLabelHeight(param) : 0;

    if (type == Type_None || type == Type_TGroup || type == Type_HGroup || type == Type_VGroup) {
        if (has_label) TextUnformatted(label);

        if (type == Type_TGroup) {
            const bool is_height_constrained = suggested_height != 0;
            // In addition to the group contents, account for the tab height and the space between the tabs and the content.
            const float group_height = max(0.f, is_height_constrained ? suggested_height - label_height : 0);
            const float item_height = max(0.f, group_height - frame_height - imgui_style.ItemSpacing.y);
            BeginTabBar(param.label.c_str());
            for (const auto &child : children) {
                if (BeginTabItem(child.label.c_str())) {
                    DrawUiItem(child, "", item_height);
                    EndTabItem();
                }
            }
            EndTabBar();
        } else {
            const float cell_padding = type == Type_None ? 0 : 2 * imgui_style.CellPadding.y;
            const bool is_h = type == Type_HGroup;
            float suggested_item_height = 0; // Including any label height, not including cell padding
            if (is_h) {
                bool include_labels = !Style.HeaderTitles;
                suggested_item_height = std::ranges::max(
                    param.children | std::views::transform([&](const auto &child) {
                        return CalcHeight(child) + (include_labels ? CalcLabelHeight(child) : 0);
                    })
                );
            }
            if (type == Type_None) { // Root group (treated as a vertical group but not as a table)
                for (const auto &child : children) DrawUiItem(child, child.label.c_str(), suggested_item_height);
            } else {
                if (BeginTable(param.id.c_str(), is_h ? int(children.size()) : 1, TableFlagsToImGui(Style.TableFlags))) {
                    const float row_min_height = suggested_item_height + cell_padding;
                    if (is_h) {
                        ParamsWidthSizingPolicy policy = Style.WidthSizingPolicy;
                        const bool allow_fixed_width_params = policy != ParamsWidthSizingPolicy_Balanced && (policy == ParamsWidthSizingPolicy_StretchFlexibleOnly || (policy == ParamsWidthSizingPolicy_StretchToFill && std::ranges::any_of(param.children, [](const auto &child) { return IsWidthExpandable(child.type); })));
                        for (const auto &child : children) {
                            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                            if (allow_fixed_width_params && !IsWidthExpandable(child.type)) flags |= ImGuiTableColumnFlags_WidthFixed;
                            TableSetupColumn(child.label.c_str(), flags, CalcWidth(child, true));
                        }
                        if (Style.HeaderTitles) {
                            // Custom headers (instead of `TableHeadersRow()`) to align column names.
                            TableNextRow(ImGuiTableRowFlags_Headers);
                            for (int column = 0; column < int(children.size()); column++) {
                                TableSetColumnIndex(column);
                                const char *column_name = TableGetColumnName(column);
                                PushID(column);
                                const float header_x = CalcAlignedX(justify.h, CalcTextSize(column_name).x, GetContentRegionAvail().x);
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
                        const string child_label = child.type == Type_Button || !is_h || !Style.HeaderTitles ? child.label : "";
                        DrawUiItem(child, child_label.c_str(), suggested_item_height);
                    }
                    EndTable();
                }
            }
        }
    } else {
        const float available_x = GetContentRegionAvail().x;
        ImVec2 item_size_no_label = {CalcWidth(param, false), CalcHeight(param)};
        ImVec2 item_size = {has_label ? CalcWidth(param, true) : item_size_no_label.x, item_size_no_label.y + label_height};
        if (IsWidthExpandable(type) && available_x > item_size.x) {
            const float expand_delta_max = available_x - item_size.x;
            const float item_width_no_label_before = item_size_no_label.x;
            item_size_no_label.x = min(Style.MaxHorizontalItemWidth * frame_height, item_size_no_label.x + expand_delta_max);
            item_size.x += item_size_no_label.x - item_width_no_label_before;
        }
        if (IsHeightExpandable(type) && suggested_height > item_size.y) item_size.y = suggested_height;
        SetNextItemWidth(item_size_no_label.x);

        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{max(0.f, CalcAlignedX(justify.h, has_label && IsLabelSameLine(type) ? item_size.x : item_size_no_label.x, available_x)), max(0.f, CalcAlignedY(justify.v, item_size.y, max(item_size.y, suggested_height)))});

        if (type == Type_Button) {
            Button(label);
            if (IsItemActivated() && *param.zone == 0.0) *param.zone = 1.0;
            else if (IsItemDeactivated() && *param.zone == 1.0) *param.zone = 0.0;
        } else if (type == Type_CheckButton) {
            auto value = bool(*param.zone);
            if (Checkbox(label, &value)) *param.zone = Real(value);
        } else if (type == Type_NumEntry) {
            auto value = int(*param.zone);
            if (InputInt(label, &value, int(param.step))) *param.zone = std::clamp(Real(value), param.min, param.max);
        } else if (type == Type_HSlider || type == Type_VSlider || type == Type_HBargraph || type == Type_VBargraph) {
            auto value = float(*param.zone);
            ValueBarFlags flags = ValueBarFlags_None;
            if (type == Type_HBargraph || type == Type_VBargraph) flags |= ValueBarFlags_ReadOnly;
            if (type == Type_VBargraph || type == Type_VSlider) flags |= ValueBarFlags_Vertical;
            if (!has_label) flags |= ValueBarFlags_NoTitle;
            if (ValueBar(param.label.c_str(), &value, item_size.y - label_height, float(param.min), float(param.max), flags, justify.h)) *param.zone = Real(value);
        } else if (type == Type_Knob) {
            auto value = float(*param.zone);
            KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
            const int steps = param.step == 0 ? 0 : int((param.max - param.min) / param.step);
            if (Knob(param.label.c_str(), &value, float(param.min), float(param.max), 0, nullptr, justify.h, steps == 0 || steps > 10 ? KnobVariant_WiperDot : KnobVariant_Stepped, flags, steps)) {
                *param.zone = Real(value);
            }
        } else if (type == Type_HRadioButtons || type == Type_VRadioButtons) {
            auto value = float(*param.zone);
            RadioButtonsFlags flags = has_label ? RadioButtonsFlags_None : RadioButtonsFlags_NoTitle;
            if (type == Type_VRadioButtons) flags |= ValueBarFlags_Vertical;
            SetNextItemWidth(item_size.x); // Include label in param width for radio buttons (inconsistent but just makes things easier).
            if (RadioButtons(param.label.c_str(), &value, Ui->NamesAndValues[param.zone], flags, justify)) *param.zone = Real(value);
        } else if (type == Type_Menu) {
            auto value = float(*param.zone);
            const auto &names_and_values = Ui->NamesAndValues[param.zone];
            // todo handle not present
            const auto selected_index = find(names_and_values.values.begin(), names_and_values.values.end(), value) - names_and_values.values.begin();
            if (BeginCombo(param.label.c_str(), names_and_values.names[selected_index].c_str())) {
                for (int i = 0; i < int(names_and_values.names.size()); i++) {
                    const Real choice_value = names_and_values.values[i];
                    const bool is_selected = value == choice_value;
                    if (Selectable(names_and_values.names[i].c_str(), is_selected)) *param.zone = Real(choice_value);
                }
                EndCombo();
            }
        }
    }
    if (param.tooltip && IsItemHovered()) {
        // todo a few issues here:
        //  - only leaf params, so group tooltips don't work.
        //  - should be either title hover or ? help marker, but if the latter, would need to account for it in width calcs
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(param.tooltip);
        EndTooltip();
    }
}

void FaustParams::Render() const {
    if (!Ui) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    DrawUiItem(Ui->UiParam, "", GetContentRegionAvail().y);

    //    if (hovered_node) {
    //        const string label = get_ui_label(hovered_node->tree);
    //        if (!label.empty()) {
    //            const auto *widget = Ui->GetWidget(label);
    //            if (widget) cout << "Found widget: " << label << '\n';
    //        }
    //    }
}

void FaustParams::OnDspChanged(dsp *dsp) {
    if (dsp) {
        Ui = std::make_unique<FaustParamsUI>();
        dsp->buildUserInterface(Ui.get());
    } else {
        Ui = nullptr;
    }
}

void FaustParams::Style::Render() const {
    HeaderTitles.Draw();
    MinHorizontalItemWidth.Draw();
    MaxHorizontalItemWidth.Draw();
    MinVerticalItemHeight.Draw();
    MinKnobItemSize.Draw();
    AlignmentHorizontal.Draw();
    AlignmentVertical.Draw();
    Spacing();
    WidthSizingPolicy.Draw();
    TableFlags.Draw();
}
