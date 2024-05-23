#include "Enum.h"

#include <ranges>

#include "imgui.h"

using std::ranges::to;

Enum::Enum(ComponentArgs &&args, std::vector<std::string> names, int value)
    : Primitive(std::move(args), value), Names(std::move(names)) {}
Enum::Enum(ComponentArgs &&args, std::function<std::string(int)> get_name, int value)
    : Primitive(std::move(args), value), Names({}), GetName(std::move(get_name)) {}

void Enum::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Primitive::Enum::Set &a) { Set(a.value); },
        },
        action
    );
}

std::string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

using namespace ImGui;

void Enum::Render() const {
    Render(std::views::iota(0, int(Names.size())) | to<std::vector>()); // todo if I stick with this pattern, cache.
}
void Enum::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    if (const int value = Value; BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
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
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) IssueSet(int(i));
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}
