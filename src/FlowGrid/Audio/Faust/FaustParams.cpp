#include "FaustParams.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/max.hpp>
#include <range/v3/core.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>

#include <imgui_internal.h>

#include "../../UI/Widgets.h"
#include "../Audio.h"

using namespace ImGui;
using namespace fg;

using ItemType = FaustParams::ItemType;
using enum FaustParams::ItemType;
using std::min, std::max;

namespace views = ranges::views;
using ranges::to, views::transform;

FaustParams *interface;

static bool IsWidthExpandable(const ItemType type) {
    return type == ItemType_HGroup || type == ItemType_VGroup || type == ItemType_TGroup || type == ItemType_NumEntry || type == ItemType_HSlider || type == ItemType_HBargraph;
}
static bool IsHeightExpandable(const ItemType type) {
    return type == ItemType_VBargraph || type == ItemType_VSlider || type == ItemType_CheckButton;
}
static bool IsLabelSameLine(const ItemType type) {
    return type == ItemType_NumEntry || type == ItemType_HSlider || type == ItemType_HBargraph || type == ItemType_HRadioButtons || type == ItemType_Menu || type == ItemType_CheckButton;
}

// todo config to place labels above horizontal items
static float CalcItemWidth(const FaustParams::Item &item, const bool include_label) {
    const bool has_label = include_label && !item.label.empty();
    const float frame_height = GetFrameHeight();
    const float inner_spacing = GetStyle().ItemInnerSpacing.x;
    const float raw_label_width = CalcTextSize(item.label.c_str()).x;
    const float label_width = has_label ? raw_label_width : 0;
    const float label_width_with_spacing = has_label ? raw_label_width + inner_spacing : 0;

    switch (item.type) {
        case ItemType_NumEntry:
        case ItemType_HSlider:
        case ItemType_HBargraph: return audio.Faust.Params.Style.MinHorizontalItemWidth * frame_height + label_width_with_spacing;
        case ItemType_HRadioButtons: {
            return label_width_with_spacing +
                ranges::accumulate(interface->names_and_values[item.zone].names | transform(CalcRadioChoiceWidth), 0.f) +
                inner_spacing * float(interface->names_and_values.size());
        }
        case ItemType_Menu: {
            return label_width_with_spacing + ranges::max(interface->names_and_values[item.zone].names | transform([](const string &choice_name) {
                                                              return CalcTextSize(choice_name).x;
                                                          })) +
                GetStyle().FramePadding.x * 2 + frame_height; // Extra frame for button
        }
        case ItemType_CheckButton: return frame_height + label_width_with_spacing;
        case ItemType_VBargraph:
        case ItemType_VSlider: return max(frame_height, label_width);
        case ItemType_VRadioButtons: return max(ranges::max(interface->names_and_values[item.zone].names | transform(CalcRadioChoiceWidth)), label_width);
        case ItemType_Button: return raw_label_width + GetStyle().FramePadding.x * 2; // Button uses label width even if `include_label == false`.
        case ItemType_Knob: return max(audio.Faust.Params.Style.MinKnobItemSize * frame_height, label_width);
        default: return GetContentRegionAvail().x;
    }
}
static float CalcItemHeight(const FaustParams::Item &item) {
    const float frame_height = GetFrameHeight();
    switch (item.type) {
        case ItemType_VBargraph:
        case ItemType_VSlider:
        case ItemType_VRadioButtons: return audio.Faust.Params.Style.MinVerticalItemHeight * frame_height;
        case ItemType_HSlider:
        case ItemType_NumEntry:
        case ItemType_HBargraph:
        case ItemType_Button:
        case ItemType_CheckButton:
        case ItemType_HRadioButtons:
        case ItemType_Menu: return frame_height;
        case ItemType_Knob: return audio.Faust.Params.Style.MinKnobItemSize * frame_height + frame_height + GetStyle().ItemSpacing.y;
        default: return 0;
    }
}
// Returns _additional_ height needed to accommodate a label for the item
static float CalcItemLabelHeight(const FaustParams::Item &item) {
    switch (item.type) {
        case ItemType_VBargraph:
        case ItemType_VSlider:
        case ItemType_VRadioButtons:
        case ItemType_Knob:
        case ItemType_HGroup:
        case ItemType_VGroup:
        case ItemType_TGroup: return GetTextLineHeightWithSpacing();
        case ItemType_Button:
        case ItemType_HSlider:
        case ItemType_NumEntry:
        case ItemType_HBargraph:
        case ItemType_CheckButton:
        case ItemType_HRadioButtons:
        case ItemType_Menu:
        case ItemType_None: return 0;
    }
}

// `suggested_height` may be positive if the item is within a constrained layout setting.
// `suggested_height == 0` means no height suggestion.
// For _items_ (as opposed to groups), the suggested height is the expected _available_ height in the group
// (which is relevant for aligning items relative to other items in the same group).
// Items/groups are allowed to extend beyond this height if needed to fit its contents.
// It is expected that the cursor position will be set appropriately below the drawn contents.
void DrawUiItem(const FaustParams::Item &item, const char *label, const float suggested_height) {
    const auto &style = GetStyle();
    const auto &params_style = audio.Faust.Params.Style;
    const Justify justify = {params_style.AlignmentHorizontal, params_style.AlignmentVertical};
    const auto type = item.type;
    const auto &children = item.items;
    const float frame_height = GetFrameHeight();
    const bool has_label = strlen(label) > 0;
    const float label_height = has_label ? CalcItemLabelHeight(item) : 0;

    if (type == ItemType_None || type == ItemType_TGroup || type == ItemType_HGroup || type == ItemType_VGroup) {
        if (has_label) TextUnformatted(label);

        if (type == ItemType_TGroup) {
            const bool is_height_constrained = suggested_height != 0;
            // In addition to the group contents, account for the tab height and the space between the tabs and the content.
            const float group_height = max(0.f, is_height_constrained ? suggested_height - label_height : 0);
            const float item_height = max(0.f, group_height - frame_height - style.ItemSpacing.y);
            BeginTabBar(item.label.c_str());
            for (const auto &child : children) {
                if (BeginTabItem(child.label.c_str())) {
                    DrawUiItem(child, "", item_height);
                    EndTabItem();
                }
            }
            EndTabBar();
        } else {
            const float cell_padding = type == ItemType_None ? 0 : 2 * style.CellPadding.y;
            const bool is_h = type == ItemType_HGroup;
            float suggested_item_height = 0; // Including any label height, not including cell padding
            if (is_h) {
                bool include_labels = !params_style.HeaderTitles;
                suggested_item_height = ranges::max(item.items | transform([include_labels](const auto &child) {
                                                        return CalcItemHeight(child) + (include_labels ? CalcItemLabelHeight(child) : 0);
                                                    }));
            }
            if (type == ItemType_None) { // Root group (treated as a vertical group but not as a table)
                for (const auto &child : children) DrawUiItem(child, child.label.c_str(), suggested_item_height);
            } else {
                if (BeginTable(item.id.c_str(), is_h ? int(children.size()) : 1, TableFlagsToImGui(params_style.TableFlags))) {
                    const float row_min_height = suggested_item_height + cell_padding;
                    if (is_h) {
                        ParamsWidthSizingPolicy policy = params_style.WidthSizingPolicy;
                        const bool allow_fixed_width_items = policy != ParamsWidthSizingPolicy_Balanced && (policy == ParamsWidthSizingPolicy_StretchFlexibleOnly || (policy == ParamsWidthSizingPolicy_StretchToFill && ranges::any_of(item.items, [](const auto &child) { return IsWidthExpandable(child.type); })));
                        for (const auto &child : children) {
                            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                            if (allow_fixed_width_items && !IsWidthExpandable(child.type)) flags |= ImGuiTableColumnFlags_WidthFixed;
                            TableSetupColumn(child.label.c_str(), flags, CalcItemWidth(child, true));
                        }
                        if (params_style.HeaderTitles) {
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
                        const string child_label = child.type == ItemType_Button || !is_h || !params_style.HeaderTitles ? child.label : "";
                        DrawUiItem(child, child_label.c_str(), suggested_item_height);
                    }
                    EndTable();
                }
            }
        }
    } else {
        const float available_x = GetContentRegionAvail().x;
        ImVec2 item_size_no_label = {CalcItemWidth(item, false), CalcItemHeight(item)};
        ImVec2 item_size = {has_label ? CalcItemWidth(item, true) : item_size_no_label.x, item_size_no_label.y + label_height};
        if (IsWidthExpandable(type) && available_x > item_size.x) {
            const float expand_delta_max = available_x - item_size.x;
            const float item_width_no_label_before = item_size_no_label.x;
            item_size_no_label.x = min(params_style.MaxHorizontalItemWidth * frame_height, item_size_no_label.x + expand_delta_max);
            item_size.x += item_size_no_label.x - item_width_no_label_before;
        }
        if (IsHeightExpandable(type) && suggested_height > item_size.y) item_size.y = suggested_height;
        SetNextItemWidth(item_size_no_label.x);

        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{max(0.f, CalcAlignedX(justify.h, has_label && IsLabelSameLine(type) ? item_size.x : item_size_no_label.x, available_x)), max(0.f, CalcAlignedY(justify.v, item_size.y, max(item_size.y, suggested_height)))});

        if (type == ItemType_Button) {
            Button(label);
            if (IsItemActivated() && *item.zone == 0.0) *item.zone = 1.0;
            else if (IsItemDeactivated() && *item.zone == 1.0) *item.zone = 0.0;
        } else if (type == ItemType_CheckButton) {
            auto value = bool(*item.zone);
            if (Checkbox(label, &value)) *item.zone = Real(value);
        } else if (type == ItemType_NumEntry) {
            auto value = int(*item.zone);
            if (InputInt(label, &value, int(item.step))) *item.zone = std::clamp(Real(value), item.min, item.max);
        } else if (type == ItemType_HSlider || type == ItemType_VSlider || type == ItemType_HBargraph || type == ItemType_VBargraph) {
            auto value = float(*item.zone);
            ValueBarFlags flags = ValueBarFlags_None;
            if (type == ItemType_HBargraph || type == ItemType_VBargraph) flags |= ValueBarFlags_ReadOnly;
            if (type == ItemType_VBargraph || type == ItemType_VSlider) flags |= ValueBarFlags_Vertical;
            if (!has_label) flags |= ValueBarFlags_NoTitle;
            if (ValueBar(item.label.c_str(), &value, item_size.y - label_height, float(item.min), float(item.max), flags, justify.h)) *item.zone = Real(value);
        } else if (type == ItemType_Knob) {
            auto value = float(*item.zone);
            KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
            const int steps = item.step == 0 ? 0 : int((item.max - item.min) / item.step);
            if (Knob(item.label.c_str(), &value, float(item.min), float(item.max), 0, nullptr, justify.h, steps == 0 || steps > 10 ? KnobVariant_WiperDot : KnobVariant_Stepped, flags, steps)) {
                *item.zone = Real(value);
            }
        } else if (type == ItemType_HRadioButtons || type == ItemType_VRadioButtons) {
            auto value = float(*item.zone);
            const auto &names_and_values = interface->names_and_values[item.zone];
            RadioButtonsFlags flags = has_label ? RadioButtonsFlags_None : RadioButtonsFlags_NoTitle;
            if (type == ItemType_VRadioButtons) flags |= ValueBarFlags_Vertical;
            SetNextItemWidth(item_size.x); // Include label in item width for radio buttons (inconsistent but just makes things easier).
            if (RadioButtons(item.label.c_str(), &value, names_and_values, flags, justify)) *item.zone = Real(value);
        } else if (type == ItemType_Menu) {
            auto value = float(*item.zone);
            const auto &names_and_values = interface->names_and_values[item.zone];
            // todo handle not present
            const auto selected_index = find(names_and_values.values.begin(), names_and_values.values.end(), value) - names_and_values.values.begin();
            if (BeginCombo(item.label.c_str(), names_and_values.names[selected_index].c_str())) {
                for (int i = 0; i < int(names_and_values.names.size()); i++) {
                    const Real choice_value = names_and_values.values[i];
                    const bool is_selected = value == choice_value;
                    if (Selectable(names_and_values.names[i].c_str(), is_selected)) *item.zone = Real(choice_value);
                }
                EndCombo();
            }
        }
    }
    if (item.tooltip && IsItemHovered()) {
        // todo a few issues here:
        //  - only items, so group tooltips don't work.
        //  - should be either title hover or ? help marker, but if the latter, would need to account for it in width calcs
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(item.tooltip);
        EndTooltip();
    }
}

void Audio::Faust::FaustParams::Render() const {
    if (!interface) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    DrawUiItem(interface->ui, "", GetContentRegionAvail().y);

    //    if (hovered_node) {
    //        const string label = get_ui_label(hovered_node->tree);
    //        if (!label.empty()) {
    //            const auto *widget = interface->GetWidget(label);
    //            if (widget) cout << "Found widget: " << label << '\n';
    //        }
    //    }
}

void OnUiChange(FaustParams *ui) {
    interface = ui;
}
