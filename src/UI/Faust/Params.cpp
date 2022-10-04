#include "FaustUI.h"

#include "imgui.h"
#include "../../Context.h"

class FaustUI;

void on_ui_change(FaustUI *);

void save_box_svg(const string &path);
using namespace ImGui;
using ItemType = FaustUI::ItemType;
using
enum FaustUI::ItemType;

using std::min, std::max;

FaustUI *interface;

// todo flag for value text to follow the value like `ImGui::ProgressBar`
enum ValueBarFlags_ {
    ValueBarFlags_None = 0,
    ValueBarFlags_Vertical = 1 << 0,
    ValueBarFlags_ReadOnly = 1 << 1,
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
void ValueBar(const char *id, const char *label, float *value, const float height, const float min_value = 0, const float max_value = 1,
              const ValueBarFlags flags = ValueBarFlags_None, const Align align = {HAlign_Center, VAlign_Center}) {
    const float width = CalcItemWidth();
    const ImVec2 &size = {width, height};
    const bool is_h = !(flags & ValueBarFlags_Vertical);
    const auto &style = GetStyle();
    const auto &draw_list = GetWindowDrawList();
    const auto &pos = GetCursorScreenPos();

    if (flags & ValueBarFlags_ReadOnly) {
        const float fraction = (*value - min_value) / max_value;
        draw_list->AddRectFilled(pos, pos + size, GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
        draw_list->AddRectFilled(
            pos + ImVec2{0, is_h ? 0 : (1 - fraction) * size.y},
            pos + size * ImVec2{is_h ? fraction : 1, 1},
            GetColorU32(ImGuiCol_PlotHistogram),
            style.FrameRounding, is_h ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersBottom
        );
    } else {
        // Draw ImGui widget without value or label text.
        if (is_h) SliderFloat(id, value, min_value, max_value, "");
        else VSliderFloat(id, size, value, min_value, max_value, "");
    }

    const string value_text = format("{:.2f}", *value);
    const float value_text_w = CalcTextSize(value_text.c_str()).x;
    const float value_text_x = align.x == HAlign_Left ? 0 : align.x == HAlign_Center ? (size.x - value_text_w) / 2 : -value_text_w + size.x;
    draw_list->AddText(pos + ImVec2{value_text_x, (size.y - GetFontSize()) / 2}, GetColorU32(ImGuiCol_Text), value_text.c_str());
    if (label) {
        const float label_w = strlen(label) > 0 ? CalcTextSize(label).x : 0;
        const float label_x = is_h ? size.x + style.ItemInnerSpacing.x : align.x == HAlign_Left ? 0 : align.x == HAlign_Center ? (size.x - label_w) / 2 : -label_w + size.x;
        draw_list->AddText(pos + ImVec2{label_x, style.FramePadding.y + (is_h ? 0 : size.y)}, GetColorU32(ImGuiCol_Text), label);
    }
}

float CalcItemWidth(const ItemType type, const char *label, const float available_width, const bool include_label = false) {
    const float label_width = label && strlen(label) > 0 ? CalcTextSize(label).x + GetStyle().FramePadding.x * 2 : 0;

    switch (type) {
        case FaustUI::ItemType_None:return 0;
        case FaustUI::ItemType_HSlider:
        case FaustUI::ItemType_NumEntry:
        case FaustUI::ItemType_HBargraph:return available_width - (include_label ? 0 : label_width);
        case FaustUI::ItemType_VBargraph:
        case FaustUI::ItemType_VSlider:
        case FaustUI::ItemType_CheckButton:return GetFrameHeight();
        case FaustUI::ItemType_Button:return label_width;
        case FaustUI::ItemType_HGroup:
        case FaustUI::ItemType_VGroup:
        case FaustUI::ItemType_TGroup:return available_width;
    }
}
float CalcItemHeight(const ItemType type, const char *label, const float available_height, const bool include_label = false) {
    const float label_height = label && strlen(label) > 0 ? GetFrameHeight() : 0;

    switch (type) {
        case FaustUI::ItemType_None:return 0;
        case FaustUI::ItemType_VBargraph:
        case FaustUI::ItemType_VSlider:return available_height - (include_label ? 0 : label_height);
        case FaustUI::ItemType_HSlider:
        case FaustUI::ItemType_NumEntry:
        case FaustUI::ItemType_HBargraph:
        case FaustUI::ItemType_CheckButton:
        case FaustUI::ItemType_Button:return GetFrameHeight();
        case FaustUI::ItemType_HGroup:
        case FaustUI::ItemType_VGroup:
        case FaustUI::ItemType_TGroup:return available_height;
    }
}

// Width is determined from `GetContentRegionAvail()`
void DrawUiItem(const FaustUI::Item &item, const float height, const ItemType parent_type = ItemType_None) {
    const static auto group_bg_color = GetColorU32(ImGuiCol_FrameBg, 0.2); // todo new FG style color

    const float width = GetContentRegionAvail().x;
    const auto &style = GetStyle();
    const auto &fg_style = s.Style.FlowGrid;
    const auto type = item.type;
    const char *label = item.label.c_str();
    const bool show_label = type == ItemType_Button || (parent_type != ItemType_TGroup && !(parent_type == ItemType_HGroup && fg_style.ParamsHeaderTitles));
    const auto &inner_items = item.items;
    const float frame_height = GetFrameHeight();

    if (type == ItemType_TGroup || type == ItemType_HGroup || type == ItemType_VGroup) {
        if (show_label) Text("%s", label);
        const float group_height = height - (show_label ? GetTextLineHeightWithSpacing() : 0);

        if (type == ItemType_TGroup) {
            BeginTabBar(label);
            for (const auto &inner_item: inner_items) {
                if (BeginTabItem(inner_item.label.c_str())) {
                    // In addition to the group contents, account for the tab height and the space between the tabs and the content.
                    DrawUiItem(inner_item, group_height - frame_height - style.ItemSpacing.y, type);
                    EndTabItem();
                }
            }
            EndTabBar();
        } else {
            const bool is_h = type == ItemType_HGroup;
            const ImVec2 row_size = {
                width,
                // Ensure the row is at least big enough to fit two frames.
                max(
                    2 * frame_height + 2 * style.CellPadding.y,
                    is_h ? (group_height - (fg_style.ParamsHeaderTitles ? GetFontSize() + 2 * style.CellPadding.y : 0)) : group_height / float(inner_items.size())
                )
            };
            if (BeginTable(label, is_h ? int(inner_items.size()) : 1, TableFlagsToImgui(fg_style.ParamsTableFlags))) {
                if (is_h) {
                    for (const auto &inner_item: inner_items) TableSetupColumn(inner_item.label.c_str());
                    if (fg_style.ParamsHeaderTitles) TableHeadersRow();
                    TableNextRow(ImGuiTableRowFlags_None, row_size.y);
                }
                const float cell_height = row_size.y - style.CellPadding.y * 2;
                for (const auto &inner_item: inner_items) {
                    if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_size.y);
                    TableNextColumn();
                    TableSetBgColor(ImGuiTableBgTarget_RowBg0, group_bg_color);
                    DrawUiItem(inner_item, cell_height, type);
                }
                EndTable();
            }
        }
    } else {
        const char *title = show_label ? label : "";
        SetNextItemWidth(CalcItemWidth(type, title, width));

        const ImVec2i alignment = {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical};
        const ImVec2 item_size_with_label = {CalcItemWidth(type, title, width, true), CalcItemHeight(type, title, height, true)}; // Includes label space

        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{
            alignment.x == HAlign_Left ? 0 : alignment.x == HAlign_Center ? (width - item_size_with_label.x) / 2 : width - item_size_with_label.x,
            alignment.y == VAlign_Top ? 0 : alignment.y == VAlign_Center ? (height - item_size_with_label.y) / 2 : height - item_size_with_label.y
        });

        if (type == ItemType_Button) {
            *item.zone = Real(Button(title));
        } else if (type == ItemType_CheckButton) {
            auto checked = bool(*item.zone);
            Checkbox(title, &checked);
            *item.zone = Real(checked);
        } else if (type == ItemType_NumEntry) {
            auto value = float(*item.zone);
            InputFloat(title, &value, float(item.step));
            *item.zone = Real(value);
        } else if (type == ItemType_HSlider || type == ItemType_VSlider || type == ItemType_HBargraph || type == ItemType_VBargraph) {
            auto value = float(*item.zone);

            ValueBarFlags flags = ValueBarFlags_None;
            if (type == ItemType_HBargraph || type == ItemType_VBargraph) flags |= ValueBarFlags_ReadOnly;
            if (type == ItemType_VBargraph || type == ItemType_VSlider) flags |= ValueBarFlags_Vertical;
            const string id = format("##{}", label);
            ValueBar(id.c_str(), title, &value, CalcItemHeight(type, title, height), float(item.min), float(item.max), flags, {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical});
            if (!(flags & ValueBarFlags_ReadOnly)) *item.zone = Real(value);
        }
        SetCursorPos(old_cursor);
    }
}

void Audio::FaustState::FaustParams::draw() const {
    if (!interface) {
        // todo don't show empty menu bar in this case
        Text("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    const float item_height = GetContentRegionAvail().y / float(interface->ui.size());
    for (const auto &item: interface->ui) DrawUiItem(item, item_height);

//    if (hovered_node) {
//        const string label = get_ui_label(hovered_node->tree);
//        if (!label.empty()) {
//            const auto *widget = interface->get_widget(label);
//            if (widget) {
//                cout << "Found widget: " << label << '\n';
//            }
//        }
//    }
}

void on_ui_change(FaustUI *ui) {
    interface = ui;
}
