#include "FaustUI.h"

#include "imgui.h"
#include "../../State.h"

class FaustUI;

void on_ui_change(FaustUI *);

void save_box_svg(const string &path);
using namespace ImGui;
using ItemType = FaustUI::ItemType;
using enum FaustUI::ItemType;

static constexpr ItemType GroupItems[]{
    ItemType_HGroup,
    ItemType_VGroup,
    ItemType_TGroup,
};
static constexpr ItemType ShortItems[]{
    ItemType_Button,
    ItemType_CheckButton,
    ItemType_HSlider,
    ItemType_NumEntry,
    ItemType_HBargraph,
    ItemType_VBargraph, // todo only for now
};
static constexpr ItemType LabeledItems[]{
    ItemType_HSlider,
    ItemType_NumEntry,
    ItemType_HBargraph,
    ItemType_VBargraph,
};

FaustUI *interface;

bool header_titles = true; // todo style config
bool center_vertical = true; // todo style config

void DrawUiItem(const FaustUI::Item &item, const ImVec2 &size, const ItemType parent_type = ItemType_None) {
    const static auto group_bg_color = GetColorU32(ImGuiCol_FrameBg, 0.2); // todo new FG style color

    const auto type = item.type;
    const char *label = item.label.c_str();
    const bool show_label = parent_type != ItemType_TGroup && !(parent_type == ItemType_HGroup && header_titles);
    const auto &inner_items = item.items;
    const bool is_group = std::find(std::begin(GroupItems), std::end(GroupItems), type) != std::end(GroupItems);

    if (is_group) {
        if (show_label) Text("%s", label);
        const float group_height = size.y - (show_label ? GetTextLineHeightWithSpacing() : 0);
        if (type == ItemType_HGroup) {
            if (BeginTable(label, int(inner_items.size()), ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                const ImVec2 item_size = {size.x / float(inner_items.size()), group_height };
                for (const auto &inner_item: inner_items) TableSetupColumn(inner_item.label.c_str());
                if (header_titles) TableHeadersRow();
                for (const auto &inner_item : inner_items) {
                    TableNextColumn();
                    TableSetBgColor(ImGuiTableBgTarget_RowBg0, group_bg_color);
                    DrawUiItem(inner_item, item_size, type);
                }
                EndTable();
            }
        } else if (type == ItemType_VGroup) {
            if (BeginTable(label, 1, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                const ImVec2 item_size = {size.x, group_height / float(inner_items.size())};
                for (const auto &inner_item : inner_items) {
                    TableNextRow(ImGuiTableRowFlags_None, item_size.y);
                    TableSetColumnIndex(0);
                    TableSetBgColor(ImGuiTableBgTarget_RowBg0, group_bg_color);
                    DrawUiItem(inner_item, item_size, type);
                }
                EndTable();
            }
        } else if (type == ItemType_TGroup) {
            BeginTabBar(label);
            for (const auto &inner_item: inner_items) {
                if (BeginTabItem(inner_item.label.c_str())) {
                    DrawUiItem(inner_item, {size.x, group_height - GetTextLineHeightWithSpacing()}, type);
                    EndTabItem();
                }
            }
            EndTabBar();
        }
    } else {
        const bool labeled = show_label && std::find(std::begin(LabeledItems), std::end(LabeledItems), type) != std::end(LabeledItems);
        SetNextItemWidth(GetContentRegionAvail().x - (labeled ? (CalcTextSize(label).x + GetFontSize()) : 0));
        const float before_y = GetCursorPosY();
        const bool is_short = std::find(std::begin(ShortItems), std::end(ShortItems), type) != std::end(ShortItems);
        const bool should_center_vertical = is_short && center_vertical;
        if (should_center_vertical) SetCursorPosY(before_y + (size.y - GetTextLineHeightWithSpacing()) / 2);

        const char *title = show_label ? label : "";
        if (type == ItemType_Button) {
            *item.zone = Real(Button(label));
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
            VSliderFloat(title, {GetFontSize() * 2, size.y}, &value, float(item.min), float(item.max), "%.1f");
            *item.zone = Real(value);
        } else if (type == ItemType_NumEntry) {
            auto value = float(*item.zone);
            InputFloat(title, &value, float(item.step));
            *item.zone = Real(value);
        } else if (type == ItemType_HBargraph) {
            const auto value = float(*item.zone);
            ProgressBar((value - float(item.min)) / float(item.max), {0, 0}, format("{:.2f}", value).c_str());
            if (show_label) {
                SameLine();
                Text("%s", title);
            }
        } else if (type == ItemType_VBargraph) {
            // todo https://github.com/ocornut/imgui/issues/5263
            const auto value = float(*item.zone);
            ProgressBar((value - float(item.min)) / float(item.max), {0, 0}, format("{:.2f}", value).c_str());
            if (show_label) {
                SameLine();
                Text("%s", title);
            }
        }
        if (should_center_vertical) SetCursorPosY(before_y);
    }
}

void Audio::FaustState::FaustParams::draw() const {
    if (!interface) {
        // todo don't show empty menu bar in this case
        Text("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    for (const auto &item: interface->ui) DrawUiItem(item, GetWindowSize());

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
