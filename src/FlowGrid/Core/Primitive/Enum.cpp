#include "Enum.h"

#include "imgui.h"

#include <__ranges/iota_view.h>
#include <range/v3/range/conversion.hpp>

Enum::Enum(ComponentArgs &&args, std::vector<string> names, int value)
    : Primitive(std::move(args), value), Names(std::move(names)) {}
Enum::Enum(ComponentArgs &&args, std::function<string(int)> get_name, int value)
    : Primitive(std::move(args), value), Names({}), GetName(std::move(get_name)) {}

void Enum::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::Enum::Set &a) { Set(a.value); },
    );
}

string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

using namespace ImGui;

void Enum::Render() const {
    Render(std::views::iota(0, int(Names.size())) | ranges::to<std::vector>); // todo if I stick with this pattern, cache.
}
void Enum::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Enum::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (u32 i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) IssueSet(int(i));
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}
