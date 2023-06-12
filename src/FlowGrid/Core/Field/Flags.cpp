#include "Flags.h"

#include "imgui.h"

#include "UI/HelpMarker.h"

Flags::Flags(ComponentArgs &&args, std::vector<Item> items, int value)
    : TypedField(std::move(args), value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto meta = Component::Metadata::Parse(name_and_help);
    Name = meta.Name;
    Help = meta.Help;
}

using namespace ImGui;

void Flags::Render() const {
    const int value = Value;
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) Action::Primitive::Set{Path, value ^ option_mask}.q(); // Toggle bit
            if (!item.Help.empty()) {
                SameLine();
                fg::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
}
void Flags::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                fg::HelpMarker(item.Help.c_str());
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) Action::Primitive::Set{Path, value ^ option_mask}.q(); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}