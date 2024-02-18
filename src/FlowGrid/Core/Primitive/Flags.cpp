#include "Flags.h"

#include "imgui.h"

#include "UI/HelpMarker.h"

Flags::Flags(ComponentArgs &&args, std::vector<Item> items, int value)
    : Primitive(std::move(args), value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto meta = Component::Metadata::Parse(name_and_help);
    Name = meta.Name;
    Help = meta.Help;
}

void Flags::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Primitive::Flags::Set &a) { Set(a.value); },
        },
        action
    );
}

using namespace ImGui;

void Flags::Render() const {
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) IssueSet(value ^ option_mask); // Toggle bit
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
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                fg::HelpMarker(item.Help);
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) IssueSet(value ^ option_mask); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}
