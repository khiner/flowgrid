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
// **Assumes the current cursor position is the desired top-left of the rectangle.**
// **Assumes the current item width has been set to the desired rectangle width.**
void ValueBar(const char *label, float *value, const float rect_height, const float min_value = 0, const float max_value = 1,
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
        SetCursorPosY(GetCursorPosY() + style.FramePadding.y);
    } else {
        // Draw ImGui widget without value or label text.
        const string &id = format("##{}", label);
        if (is_h) SliderFloat(id.c_str(), value, min_value, max_value, "");
        else VSliderFloat(id.c_str(), rect_size, value, min_value, max_value, "");
    }

    const string value_text = format("{:.2f}", *value);
    const float value_text_w = CalcTextSize(value_text.c_str()).x;
    const float value_text_x = is_h || align.x == HAlign_Center ? (rect_size.x - value_text_w) / 2 : align.x == HAlign_Left ? 0 : -value_text_w + rect_size.x;
    draw_list->AddText(rect_pos + ImVec2{value_text_x, (rect_size.y - GetFontSize()) / 2}, GetColorU32(ImGuiCol_Text), value_text.c_str());

    if (has_title) {
        if (is_h) SameLine();
        else SetCursorPos(cursor);

        Text("%s", label);
    }

    EndGroup();
    PopID();
}

float CalcLabelWidth(const string &text) {
    if (text.empty()) return 0;
    return CalcTextSize(text.c_str()).x + GetStyle().FramePadding.x * 2;
}

float CalcItemWidth(const ItemType type, const string &label, bool include_label) {
    const float label_width = CalcLabelWidth(label);
    const float frame_height = GetFrameHeight();
    switch (type) {
        // todo impose min-width for horizontal items (more for `NumEntry`)
        // todo config to place labels above horizontal items
        case ItemType_NumEntry:
        case ItemType_HSlider:
        case ItemType_HBargraph:return s.Style.FlowGrid.ParamsMinHorizontalItemWidth * frame_height + (include_label ? label_width : 0);
        case ItemType_VBargraph:
        case ItemType_VSlider:return max(frame_height, include_label ? label_width : 0);
        case ItemType_CheckButton:return frame_height + (include_label ? label_width : 0);
        case ItemType_Button:return label_width;
        case ItemType_Knob:return max(s.Style.FlowGrid.ParamsMinKnobItemSize * frame_height, include_label ? label_width : 0);
        case ItemType_HGroup:
        case ItemType_VGroup:
        case ItemType_TGroup:
        case ItemType_None:return GetContentRegionAvail().x;
    }
}
float CalcItemHeight(const ItemType type, bool include_label) {
    const float frame_height = GetFrameHeight();
    switch (type) {
        case ItemType_VBargraph:
        case ItemType_VSlider:return s.Style.FlowGrid.ParamsMinVerticalItemHeight * frame_height + (include_label ? frame_height : 0);
        case ItemType_HSlider:
        case ItemType_NumEntry:
        case ItemType_HBargraph:
        case ItemType_CheckButton:
        case ItemType_Button:return frame_height;
        case ItemType_Knob:return s.Style.FlowGrid.ParamsMinKnobItemSize * frame_height + frame_height + (include_label ? frame_height : 0);
        case ItemType_HGroup:
        case ItemType_VGroup:
        case ItemType_TGroup:
        case ItemType_None:return 0;
    }
}

#include <range/v3/algorithm/max.hpp>

static bool is_height_expandable(const ItemType type) {
    return type == ItemType_VBargraph || type == ItemType_VSlider || type == ItemType_CheckButton || type == ItemType_Button;
}
static bool is_width_expandable(const ItemType type) { return !is_height_expandable(type); }

// `suggested_height` may be positive if the item is within a constrained layout setting.
// `suggested_height == 0` means no height suggestion.
// For _items_ (as opposed to groups), the suggested height is the expected _available_ height in the group
// (which is relevant for aligning items relative to other items in the same group).
// Items/groups are allowed to extend beyond this height if needed to fit its contents.
// It is expected that the cursor position will be set appropriately below the drawn contents.
void DrawUiItem(const FaustUI::Item &item, const string &label, const float suggested_height) {
    const static auto group_bg_color = GetColorU32(ImGuiCol_FrameBg, 0.2); // todo new FG style color

    const bool is_height_constrained = suggested_height != 0;
    const auto &style = GetStyle();
    const auto &fg_style = s.Style.FlowGrid;
    const auto type = item.type;
    const auto &children = item.items;
    const float frame_height = GetFrameHeight();
    const bool has_label = !label.empty();

    if (type == ItemType_None || type == ItemType_TGroup || type == ItemType_HGroup || type == ItemType_VGroup) {
        if (has_label) Text("%s", label.c_str());

        const float group_height = max(0.0f, is_height_constrained ? suggested_height - (has_label ? GetTextLineHeightWithSpacing() : 0) : 0);
        if (type == ItemType_TGroup) {
            BeginTabBar(item.label.c_str());
            for (const auto &child: children) {
                if (BeginTabItem(child.label.c_str())) {
                    // In addition to the group contents, account for the tab height and the space between the tabs and the content.
                    DrawUiItem(child, "", max(0.0f, group_height - frame_height - style.ItemSpacing.y));
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
                suggested_item_height = ranges::max(item.items | transform([include_labels](const auto &child) { return CalcItemHeight(child.type, include_labels); }));
            }
            if (type == ItemType_None) { // Root group (treated as a vertical group but not as a table)
                for (const auto &child: children) DrawUiItem(child, child.label, suggested_item_height);
            } else {
                if (BeginTable(item.label.c_str(), is_h ? int(children.size()) : 1, TableFlagsToImgui(fg_style.ParamsTableFlags, fg_style.ParamsTableSizingPolicy))) {
                    const float row_min_height = suggested_item_height + cell_padding;
                    if (is_h) {
                        for (const auto &inner_item: children) {
                            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                            const auto inner_type = inner_item.type;
                            if (!is_width_expandable(inner_type)) flags |= ImGuiTableColumnFlags_WidthFixed;
                            TableSetupColumn(inner_item.label.c_str(), flags);
                        }
                        if (fg_style.ParamsHeaderTitles) TableHeadersRow();
                        TableNextRow(ImGuiTableRowFlags_None, row_min_height);
                    }
                    for (const auto &child: children) {
                        if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_min_height);
                        TableNextColumn();
                        TableSetBgColor(ImGuiTableBgTarget_RowBg0, group_bg_color);
                        const string &child_label = child.type == ItemType_Button || !is_h || !fg_style.ParamsHeaderTitles ? child.label : "";
                        DrawUiItem(child, child_label, suggested_item_height);
                    }
                    EndTable();
                }
            }
        }
    } else {
        ImVec2 item_size_no_label = {CalcItemWidth(type, item.label, false), CalcItemHeight(type, false)};
        if (is_height_expandable(type) && suggested_height > item_size_no_label.y) item_size_no_label.y = suggested_height;
        const ImVec2 &item_size_with_label = has_label ? ImVec2{CalcItemWidth(type, item.label, true), CalcItemHeight(type, true)} : item_size_no_label;
        SetNextItemWidth(item_size_no_label.x);

        const float available_x = GetContentRegionAvail().x;
        const ImVec2i alignment = {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical};
        const float constrained_height = max(item_size_no_label.y, suggested_height);
        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{
            max(0.0f, alignment.x == HAlign_Left ? 0 : alignment.x == HAlign_Center ? (available_x - item_size_with_label.x) / 2 : available_x - item_size_with_label.x),
            alignment.y == VAlign_Top ? 0 : alignment.y == VAlign_Center ? (constrained_height - item_size_with_label.y) / 2 : constrained_height - item_size_with_label.y
        });

        if (type == ItemType_Button) {
            *item.zone = Real(Button(label.c_str()));
        } else if (type == ItemType_CheckButton) {
            auto checked = bool(*item.zone);
            Checkbox(label.c_str(), &checked);
            *item.zone = Real(checked);
        } else if (type == ItemType_NumEntry) {
            auto value = float(*item.zone);
            InputFloat(label.c_str(), &value, float(item.step));
            *item.zone = Real(value);
        } else if (type == ItemType_HSlider || type == ItemType_VSlider || type == ItemType_HBargraph || type == ItemType_VBargraph) {
            auto value = float(*item.zone);
            ValueBarFlags flags = ValueBarFlags_None;
            if (type == ItemType_HBargraph || type == ItemType_VBargraph) flags |= ValueBarFlags_ReadOnly;
            if (type == ItemType_VBargraph || type == ItemType_VSlider) flags |= ValueBarFlags_Vertical;
            if (!has_label) flags |= ValueBarFlags_NoTitle;

            ValueBar(item.label.c_str(), &value, item_size_no_label.y, float(item.min), float(item.max), flags, alignment);
            if (!(flags & ValueBarFlags_ReadOnly)) *item.zone = Real(value);
        } else if (type == ItemType_Knob) {
            auto value = float(*item.zone);
            KnobFlags flags = has_label ? KnobFlags_None : KnobFlags_NoTitle;
            const int steps = item.step == 0 ? 0 : int((item.max - item.min) / item.step);
            Knobs::Knob(item.label.c_str(), &value, float(item.min), float(item.max), 0, nullptr, steps == 0 || steps > 10 ? KnobVariant_WiperDot : KnobVariant_Stepped, flags, steps);
            *item.zone = Real(value);
        }
    }
}

void Audio::FaustState::FaustParams::draw() const {
    if (!interface) {
        // todo don't show empty menu bar in this case
        Text("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
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
