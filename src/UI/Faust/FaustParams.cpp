#include <range/v3/algorithm/max.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/algorithm/any_of.hpp>

#include "FaustUI.h"
#include "../../App.h"
#include "../Knob.h"


using namespace ImGui;
using ItemType = FaustUI::ItemType;
using
enum FaustUI::ItemType;

FaustUI *interface;

// todo flag for value text to follow the value like `ImGui::ProgressBar`
enum ValueBarFlags_ {
    ValueBarFlags_None = 0,
    ValueBarFlags_Vertical = 1 << 0,
    ValueBarFlags_ReadOnly = 1 << 1,
    ValueBarFlags_NoTitle = 1 << 2,
};
using ValueBarFlags = int;

// When `ReadOnly` is set, this is similar to `ImGui::ProgressBar`, but it has a horizontal/vertical switch,
// and the value text doesn't follow the value position (it stays in the middle).
// If `ReadOnly` is not set, this delegates to `SliderFloat`/`VSliderFloat`, but renders the value & label independently.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect, respecting the passed in alignment.
// `size` is the rectangle size.
// Assumes the current cursor position is either the desired top-left of the rectangle (or the beginning of the label for a vertical bar with a title).
// Assumes the current item width has been set to the desired rectangle width (not including label width).
bool ValueBar(const char *label, float *value, const float rect_height, const float min_value = 0, const float max_value = 1,
              const ValueBarFlags flags = ValueBarFlags_None, const Align align = {HAlign_Center, VAlign_Center}) {
    const float rect_width = CalcItemWidth();
    const ImVec2 &rect_size = {rect_width, rect_height};
    const auto &style = GetStyle();
    const bool is_h = !(flags & ValueBarFlags_Vertical);
    const bool has_title = !(flags & ValueBarFlags_NoTitle);
    const auto &draw_list = GetWindowDrawList();

    PushID(label);
    BeginGroup();

    const auto cursor = GetCursorPos();
    if (!is_h && has_title) {
        const float label_w = CalcTextSize(label).x;
        const float rect_x = align.x == HAlign_Left ? 0 : align.x == HAlign_Center ? (label_w - rect_size.x) / 2 : label_w - rect_size.x;
        SetCursorPos(GetCursorPos() + ImVec2{rect_x, GetTextLineHeightWithSpacing()});
    }
    const auto &rect_pos = GetCursorScreenPos();

    bool changed = false;
    if (flags & ValueBarFlags_ReadOnly) {
        const float fraction = (*value - min_value) / max_value;
        RenderFrame(rect_pos, rect_pos + rect_size, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        draw_list->AddRectFilled(
            rect_pos + ImVec2{0, is_h ? 0 : (1 - fraction) * rect_size.y},
            rect_pos + rect_size * ImVec2{is_h ? fraction : 1, 1},
            GetColorU32(ImGuiCol_PlotHistogram),
            style.FrameRounding, is_h ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersBottom
        );
        Dummy(rect_size);
    } else {
        // Draw ImGui widget without value or label text.
        const string &id = format("##{}", label);
        changed = is_h ? SliderFloat(id.c_str(), value, min_value, max_value, "") : VSliderFloat(id.c_str(), rect_size, value, min_value, max_value, "");
    }

    const string value_text = format("{:.2f}", *value);
    const float value_text_w = CalcTextSize(value_text.c_str()).x;
    const float value_text_x = is_h || align.x == HAlign_Center ? (rect_size.x - value_text_w) / 2 : align.x == HAlign_Left ? 0 : -value_text_w + rect_size.x;
    draw_list->AddText(rect_pos + ImVec2{value_text_x, (rect_size.y - GetFontSize()) / 2}, GetColorU32(ImGuiCol_Text), value_text.c_str());

    if (has_title) {
        if (is_h) SameLine();
        else SetCursorPos(cursor);

        TextUnformatted(label);
    }

    EndGroup();
    PopID();

    return !(flags & ValueBarFlags_ReadOnly) && changed; // Read-only value bars never change.
}

enum RadioButtonsFlags_ {
    RadioButtonsFlags_None = 0,
    RadioButtonsFlags_Vertical = 1 << 0,
    RadioButtonsFlags_NoTitle = 1 << 1,
};
using RadioButtonsFlags = int;

static float CalcRadioChoiceWidth(const string &choice_name) {
    return CalcTextSize(choice_name.c_str()).x + GetStyle().ItemInnerSpacing.x + GetFrameHeight();
}

// When `ReadOnly` is set, this is similar to `ImGui::ProgressBar`, but it has a horizontal/vertical switch,
// and the value text doesn't follow the value position (it stays in the middle).
// If `ReadOnly` is not set, this delegates to `SliderFloat`/`VSliderFloat`, but renders the value & label independently.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect, respecting the passed in alignment.
// `size` is the rectangle size.
// Assumes the current cursor position is either the desired top-left of the rectangle (or the beginning of the label for a vertical bar with a title).
// Assumes the current item width has been set to the desired rectangle width (not including label width).
bool RadioButtons(const char *label, float *value, const FaustUI::NamesAndValues &names_and_values, const RadioButtonsFlags flags = RadioButtonsFlags_None, const Align align = {HAlign_Center, VAlign_Center}) {
    PushID(label);
    BeginGroup();

    const auto &style = GetStyle();
    const bool is_h = !(flags & RadioButtonsFlags_Vertical);
    const float item_width = CalcItemWidth();
    if (!(flags & RadioButtonsFlags_NoTitle)) {
        const float label_width = CalcTextSize(label).x;
        ImVec2 label_pos = GetCursorScreenPos() + ImVec2{is_h ? 0 : align.x == HAlign_Left ? 0 :
                                                                    align.x == HAlign_Center ? (item_width - label_width) / 2 :
                                                                    item_width - label_width, is_h ? style.FramePadding.y : 0};
        RenderText(label_pos, label);
        Dummy({label_width, GetFrameHeight()});
    }

    bool changed = false;
    for (int i = 0; i < int(names_and_values.names.size()); i++) {
        const string &choice_name = names_and_values.names[i];
        const Real choice_value = names_and_values.values[i];
        const float choice_width = CalcRadioChoiceWidth(choice_name);
        if (!is_h) SetCursorPosX(GetCursorPosX() + (align.x == HAlign_Left ? 0 : align.x == HAlign_Center ? (item_width - choice_width) / 2 : item_width - choice_width));
        else SameLine(0, style.ItemInnerSpacing.x);

        if (RadioButton(choice_name.c_str(), *value == choice_value)) {
            *value = float(choice_value);
            changed = true;
        }
    }
    EndGroup();
    PopID();

    return changed;
}

static bool is_width_expandable(const ItemType type) {
    return type == ItemType_HGroup || type == ItemType_VGroup || type == ItemType_TGroup || type == ItemType_NumEntry || type == ItemType_HSlider || type == ItemType_HBargraph;
}
static bool is_height_expandable(const ItemType type) {
    return type == ItemType_VBargraph || type == ItemType_VSlider || type == ItemType_CheckButton;
}

// todo config to place labels above horizontal items
static float CalcItemWidth(const FaustUI::Item &item, const bool include_label) {
    const float frame_height = GetFrameHeight();
    const float inner_spacing = GetStyle().ItemInnerSpacing.x;
    const bool has_label = include_label && !item.label.empty();
    const float label_width = has_label ? CalcTextSize(item.label.c_str()).x : 0;
    const float label_width_with_spacing = has_label ? label_width + inner_spacing : 0;

    switch (item.type) {
        case ItemType_NumEntry:
        case ItemType_HSlider:
        case ItemType_HBargraph:return s.Style.FlowGrid.ParamsMinHorizontalItemWidth * frame_height + label_width_with_spacing;
        case ItemType_VBargraph:
        case ItemType_VSlider:return max(frame_height, label_width);
        case ItemType_VRadioButtons:return max(ranges::max(interface->names_and_values[item.zone].names | transform(CalcRadioChoiceWidth)), label_width);
        case ItemType_HRadioButtons:
            return label_width_with_spacing +
                ranges::accumulate(interface->names_and_values[item.zone].names | transform(CalcRadioChoiceWidth), 0.0f) +
                inner_spacing * float(interface->names_and_values.size());
        case ItemType_Menu:
            return label_width_with_spacing + ranges::max(interface->names_and_values[item.zone].names | transform([](const string &choice_name) {
                return CalcTextSize(choice_name.c_str()).x;
            })) + GetStyle().FramePadding.x * 2 + frame_height; // Extra frame for button
        case ItemType_CheckButton:return frame_height + label_width_with_spacing;
        case ItemType_Button:return label_width + GetStyle().FramePadding.x * 2; // Button uses label width even if `include_label == false`.
        case ItemType_Knob:return max(s.Style.FlowGrid.ParamsMinKnobItemSize * frame_height, label_width);
        default:return GetContentRegionAvail().x;
    }
}
static float CalcItemHeight(const FaustUI::Item &item) {
    const float frame_height = GetFrameHeight();
    switch (item.type) {
        case ItemType_VBargraph:
        case ItemType_VSlider:
        case ItemType_VRadioButtons:return s.Style.FlowGrid.ParamsMinVerticalItemHeight * frame_height;
        case ItemType_HSlider:
        case ItemType_NumEntry:
        case ItemType_HBargraph:
        case ItemType_Button:
        case ItemType_CheckButton:
        case ItemType_HRadioButtons:
        case ItemType_Menu:return frame_height;
        case ItemType_Knob:return s.Style.FlowGrid.ParamsMinKnobItemSize * frame_height + frame_height + GetStyle().ItemSpacing.y;
        default:return 0;
    }
}
// Returns _additional_ height needed to accommodate a label for the item
static float CalcItemLabelHeight(const FaustUI::Item &item) {
    switch (item.type) {
        case ItemType_VBargraph:
        case ItemType_VSlider:
        case ItemType_VRadioButtons:
        case ItemType_Knob:
        case ItemType_HGroup:
        case ItemType_VGroup:
        case ItemType_TGroup:return GetTextLineHeightWithSpacing();
        case ItemType_Button:
        case ItemType_HSlider:
        case ItemType_NumEntry:
        case ItemType_HBargraph:
        case ItemType_CheckButton:
        case ItemType_HRadioButtons:
        case ItemType_Menu:
        case ItemType_None:return 0;
    }
}

// `suggested_height` may be positive if the item is within a constrained layout setting.
// `suggested_height == 0` means no height suggestion.
// For _items_ (as opposed to groups), the suggested height is the expected _available_ height in the group
// (which is relevant for aligning items relative to other items in the same group).
// Items/groups are allowed to extend beyond this height if needed to fit its contents.
// It is expected that the cursor position will be set appropriately below the drawn contents.
void DrawUiItem(const FaustUI::Item &item, const string &label, const float suggested_height) {
    const auto &style = GetStyle();
    const auto &fg_style = s.Style.FlowGrid;
    const ImVec2i alignment = {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical};
    const auto type = item.type;
    const auto &children = item.items;
    const float frame_height = GetFrameHeight();
    const bool has_label = !label.empty();
    const float label_height = has_label ? CalcItemLabelHeight(item) : 0;

    if (type == ItemType_None || type == ItemType_TGroup || type == ItemType_HGroup || type == ItemType_VGroup) {
        if (has_label) TextUnformatted(label.c_str());

        if (type == ItemType_TGroup) {
            const bool is_height_constrained = suggested_height != 0;
            // In addition to the group contents, account for the tab height and the space between the tabs and the content.
            const float group_height = max(0.0f, is_height_constrained ? suggested_height - label_height : 0);
            const float item_height = max(0.0f, group_height - frame_height - style.ItemSpacing.y);
            BeginTabBar(item.label.c_str());
            for (const auto &child: children) {
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
                bool include_labels = !fg_style.ParamsHeaderTitles;
                suggested_item_height = ranges::max(item.items | transform([include_labels](const auto &child) {
                    return CalcItemHeight(child) + (include_labels ? CalcItemLabelHeight(child) : 0);
                }));
            }
            if (type == ItemType_None) { // Root group (treated as a vertical group but not as a table)
                for (const auto &child: children) DrawUiItem(child, child.label, suggested_item_height);
            } else {
                if (BeginTable(item.id.c_str(), is_h ? int(children.size()) : 1, TableFlagsToImgui(fg_style.ParamsTableFlags))) {
                    const float row_min_height = suggested_item_height + cell_padding;
                    if (is_h) {
                        ParamsWidthSizingPolicy policy = fg_style.ParamsWidthSizingPolicy;
                        const bool allow_fixed_width_items = policy != ParamsWidthSizingPolicy_Balanced && (policy == ParamsWidthSizingPolicy_StretchFlexibleOnly ||
                            (policy == ParamsWidthSizingPolicy_StretchToFill && ranges::any_of(item.items, [](const auto &child) { return is_width_expandable(child.type); })));
                        for (const auto &child: children) {
                            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                            if (allow_fixed_width_items && !is_width_expandable(child.type)) flags |= ImGuiTableColumnFlags_WidthFixed;
                            TableSetupColumn(child.label.c_str(), flags, CalcItemWidth(child, true));
                        }
                        if (fg_style.ParamsHeaderTitles) {
                            // Custom headers (instead of `TableHeadersRow()`) to align column names.
                            TableNextRow(ImGuiTableRowFlags_Headers);
                            for (int column = 0; column < int(children.size()); column++) {
                                TableSetColumnIndex(column);
                                const char *column_name = TableGetColumnName(column);
                                PushID(column);
                                const float header_x = alignment.x == HAlign_Left ? 0 :
                                                       alignment.x == HAlign_Center ? (GetContentRegionAvail().x - CalcTextSize(column_name).x) / 2 :
                                                       GetContentRegionAvail().x - CalcTextSize(column_name).x;
                                SetCursorPosX(GetCursorPosX() + max(0.0f, header_x));
                                TableHeader(column_name);
                                PopID();
                            }
                        }
                        TableNextRow(ImGuiTableRowFlags_None, row_min_height);
                    }
                    for (const auto &child: children) {
                        if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_min_height);
                        TableNextColumn();
                        TableSetBgColor(ImGuiTableBgTarget_RowBg0, ColorConvertFloat4ToU32(fg_style.Colors[FlowGridCol_ParamsBg]));
                        const string &child_label = child.type == ItemType_Button || !is_h || !fg_style.ParamsHeaderTitles ? child.label : "";
                        DrawUiItem(child, child_label, suggested_item_height);
                    }
                    EndTable();
                }
            }
        }
    } else {
        const float available_x = GetContentRegionAvail().x;
        ImVec2 item_size_no_label = {CalcItemWidth(item, false), CalcItemHeight(item)};
        ImVec2 item_size = {has_label ? CalcItemWidth(item, true) : item_size_no_label.x, item_size_no_label.y + label_height};
        if (is_width_expandable(type) && available_x > item_size.x) {
            const float expand_delta_max = available_x - item_size.x;
            const float item_width_no_label_before = item_size_no_label.x;
            item_size_no_label.x = min(fg_style.ParamsMaxHorizontalItemWidth.value * frame_height, item_size_no_label.x + expand_delta_max);
            item_size.x += item_size_no_label.x - item_width_no_label_before;
        }
        if (is_height_expandable(type) && suggested_height > item_size.y) item_size.y = suggested_height;
        SetNextItemWidth(item_size_no_label.x);

        const float constrained_height = max(item_size.y, suggested_height);
        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{
            max(0.0f, alignment.x == HAlign_Left ? 0 : alignment.x == HAlign_Center ? (available_x - item_size.x) / 2 : available_x - item_size.x),
            alignment.y == VAlign_Top ? 0 : alignment.y == VAlign_Center ? (constrained_height - item_size.y) / 2 : constrained_height - item_size.y
        });

        if (type == ItemType_Button) {
            *item.zone = Real(Button(label.c_str()));
        } else if (type == ItemType_CheckButton) {
            auto value = bool(*item.zone);
            if (Checkbox(label.c_str(), &value)) *item.zone = Real(value);
        } else if (type == ItemType_NumEntry) {
            auto value = float(*item.zone);
            if (InputFloat(label.c_str(), &value, float(item.step))) *item.zone = Real(value);
        } else if (type == ItemType_HSlider || type == ItemType_VSlider || type == ItemType_HBargraph || type == ItemType_VBargraph) {
            auto value = float(*item.zone);
            ValueBarFlags flags = ValueBarFlags_None;
            if (type == ItemType_HBargraph || type == ItemType_VBargraph) flags |= ValueBarFlags_ReadOnly;
            if (type == ItemType_VBargraph || type == ItemType_VSlider) flags |= ValueBarFlags_Vertical;
            if (!has_label) flags |= ValueBarFlags_NoTitle;
            if (ValueBar(item.label.c_str(), &value, item_size.y - label_height, float(item.min), float(item.max), flags, alignment)) *item.zone = Real(value);
        } else if (type == ItemType_Knob) {
            auto value = float(*item.zone);
            KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
            const int steps = item.step == 0 ? 0 : int((item.max - item.min) / item.step);
            if (Knobs::Knob(item.label.c_str(), &value, float(item.min), float(item.max), 0, nullptr,
                steps == 0 || steps > 10 ? KnobVariant_WiperDot : KnobVariant_Stepped, flags, steps)) {
                *item.zone = Real(value);
            }
        } else if (type == ItemType_HRadioButtons || type == ItemType_VRadioButtons) {
            auto value = float(*item.zone);
            const auto &names_and_values = interface->names_and_values[item.zone];
            RadioButtonsFlags flags = has_label ? RadioButtonsFlags_None : RadioButtonsFlags_NoTitle;
            if (type == ItemType_VRadioButtons) flags |= ValueBarFlags_Vertical;
            SetNextItemWidth(item_size.x); // Include label in item width for radio buttons (inconsistent but just makes things easier).
            if (RadioButtons(item.label.c_str(), &value, names_and_values, flags, alignment)) *item.zone = Real(value);
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

void Audio::FaustState::FaustParams::Draw() const {
    if (!interface) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    DrawUiItem(interface->ui, "", GetContentRegionAvail().y);

//    if (hovered_node) {
//        const string label = get_ui_label(hovered_node->tree);
//        if (!label.empty()) {
//            const auto *widget = interface->get_widget(label);
//            if (widget) cout << "Found widget: " << label << '\n';
//        }
//    }
}

void on_ui_change(FaustUI *ui) {
    interface = ui;
}
