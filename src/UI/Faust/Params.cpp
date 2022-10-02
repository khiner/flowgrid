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

static constexpr ItemType GroupItems[]{
    ItemType_HGroup,
    ItemType_VGroup,
    ItemType_TGroup,
};

FaustUI *interface;

// todo flag for value text to follow the value like `ImGui::ProgressBar`
enum ValueBarFlags_ {
    ValueBarFlags_None = 0,
    ValueBarFlags_Vertical,
};
using ValueBarFlags = int;

// Similar to `ImGui::ProgressBar`, but with a horizontal/vertical switch.
// The value text doesn't follow the value like `ImGui::ProgressBar`.
// Here it's simply displayed in the middle of the bar.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect.
// `size` is the rectangle size.
// **Assumes the current cursor position is where you want the top-left of the rectangle to be.**
void ValueBar(const float value, const char *label, const ImVec2 &size, const float min_value = 0, const float max_value = 1,
              const ValueBarFlags flags = ValueBarFlags_None, const Align align = {HAlign_Center, VAlign_Center}) {
    const bool is_h = !(flags & ValueBarFlags_Vertical);
    const auto &style = GetStyle();
    const auto &draw_list = GetWindowDrawList();
    const auto &pos = GetCursorScreenPos();
    const float fraction = (value - min_value) / max_value;

    draw_list->AddRectFilled(pos, pos + size, GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
    draw_list->AddRectFilled(
        pos + ImVec2{0, is_h ? 0 : (1 - fraction) * size.y},
        pos + size * ImVec2{is_h ? fraction : 1, 1},
        GetColorU32(ImGuiCol_PlotHistogram),
        style.FrameRounding, is_h ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersBottom
    );
    const string value_text = is_h ? format("{:.2f}", value) : format("{:.1f}", value);
    draw_list->AddText(pos + (size - CalcTextSize(value_text.c_str())) / 2, GetColorU32(ImGuiCol_Text), value_text.c_str());
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

void DrawUiItem(const FaustUI::Item &item, const ImVec2 &size, const ItemType parent_type = ItemType_None) {
    const static auto group_bg_color = GetColorU32(ImGuiCol_FrameBg, 0.2); // todo new FG style color

    const auto &style = GetStyle();
    const auto &fg_style = s.Style.FlowGrid;
    const auto type = item.type;
    const char *label = item.label.c_str();
    const bool show_label = type == ItemType_Button || (parent_type != ItemType_TGroup && !(parent_type == ItemType_HGroup && fg_style.ParamsHeaderTitles));
    const auto &inner_items = item.items;
    const bool is_group = std::find(std::begin(GroupItems), std::end(GroupItems), type) != std::end(GroupItems);
    const float frame_height = GetFrameHeight();

    if (is_group) {
        if (show_label) Text("%s", label);
        const float group_height = size.y - (show_label ? GetTextLineHeightWithSpacing() : 0);
        if (type == ItemType_HGroup || type == ItemType_VGroup) {
            const bool is_h = type == ItemType_HGroup;
            const int column_count = is_h ? 1 : int(inner_items.size());
            const float cell_frame_height = frame_height + 2 * style.CellPadding.y;
            const ImVec2 row_size = {
                size.x,
                // Ensure the row is at least big enough to fit two frames.
                max(cell_frame_height + frame_height, is_h ? (group_height - (fg_style.ParamsHeaderTitles ? cell_frame_height : 0)) : group_height / float(inner_items.size()))
            };
            if (BeginTable(label, is_h ? int(inner_items.size()) : 1, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                const ImVec2 cell_size = ImVec2{row_size.x / float(column_count), row_size.y} - style.CellPadding * 2;
                if (is_h) {
                    for (const auto &inner_item: inner_items) TableSetupColumn(inner_item.label.c_str());
                    if (fg_style.ParamsHeaderTitles) TableHeadersRow();
                    TableNextRow(ImGuiTableRowFlags_None, row_size.y);
                }
                for (const auto &inner_item: inner_items) {
                    if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_size.y);
                    TableNextColumn();
                    TableSetBgColor(ImGuiTableBgTarget_RowBg0, group_bg_color);
                    DrawUiItem(inner_item, cell_size, type);
                }
                EndTable();
            }
        } else if (type == ItemType_TGroup) {
            BeginTabBar(label);
            for (const auto &inner_item: inner_items) {
                if (BeginTabItem(inner_item.label.c_str())) {
                    DrawUiItem(inner_item, {size.x, group_height - frame_height}, type);
                    EndTabItem();
                }
            }
            EndTabBar();
        }
    } else {
        const char *title = show_label ? label : "";
        const ImVec2i alignment = {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical};
        const ImVec2 available = {GetContentRegionAvail().x, size.y}; // `GetContentRegionAvail` doesn't work for table rows, so used passed-in `size` for height.
        const ImVec2 item_size = {CalcItemWidth(type, title, available.x), CalcItemHeight(type, title, available.y)}; // Doesn't include label space
        const ImVec2 item_size_with_label = {CalcItemWidth(type, title, available.x, true), CalcItemHeight(type, title, available.y, true)}; // Includes label space
        SetNextItemWidth(item_size.x);

        const auto old_cursor = GetCursorPos();
        SetCursorPos(old_cursor + ImVec2{
            alignment.x == HAlign_Left ? 0 : alignment.x == HAlign_Center ? (available.x - item_size_with_label.x) / 2 : available.x - item_size_with_label.x,
            alignment.y == VAlign_Top ? 0 : alignment.y == VAlign_Center ? (available.y - item_size_with_label.y) / 2 : available.y - item_size_with_label.y
        });

        if (type == ItemType_Button) {
            *item.zone = Real(Button(title));
        } else if (type == ItemType_CheckButton) {
            auto checked = bool(*item.zone);
            Checkbox(title, &checked);
            *item.zone = Real(checked);
        } else if (type == ItemType_HSlider) {
            auto value = float(*item.zone);
            SliderFloat(title, &value, float(item.min), float(item.max), "%.2f");
            *item.zone = Real(value);
        } else if (type == ItemType_VSlider) {
            auto value = float(*item.zone);
            VSliderFloat(title, item_size, &value, float(item.min), float(item.max), "%.1f");
            *item.zone = Real(value);
        } else if (type == ItemType_NumEntry) {
            auto value = float(*item.zone);
            InputFloat(title, &value, float(item.step));
            *item.zone = Real(value);
        } else if (type == ItemType_HBargraph || type == ItemType_VBargraph) {
            const auto value = float(*item.zone);
            ValueBar(
                value, title, item_size, float(item.min), float(item.max),
                type == ItemType_HBargraph ? ValueBarFlags_None : ValueBarFlags_Vertical,
                {fg_style.ParamsAlignmentHorizontal, fg_style.ParamsAlignmentVertical}
            );
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

    const auto &size = GetContentRegionAvail() - GetStyle().WindowPadding;
    for (const auto &item: interface->ui) DrawUiItem(item, {size.x, size.y / float(interface->ui.size())});

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
