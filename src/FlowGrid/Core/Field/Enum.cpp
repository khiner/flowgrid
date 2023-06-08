#include "Enum.h"

#include "imgui.h"

#include <range/v3/core.hpp>
#include <range/v3/view/iota.hpp>

Enum::Enum(Stateful *parent, string_view path_segment, string_view name_help, std::vector<string> names, int value)
    : TypedField(parent, path_segment, name_help, value), Names(std::move(names)) {}
Enum::Enum(Stateful *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value)
    : TypedField(parent, path_segment, name_help, value), Names({}), GetName(std::move(get_name)) {}
string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

using namespace ImGui;

void Enum::Render() const {
    Render(ranges::views::ints(0, int(Names.size())) | ranges::to<std::vector>); // todo if I stick with this pattern, cache.
}
void Enum::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) Action::Primitive::Set{Path, option}.q();
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
        for (Count i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) Action::Primitive::Set{Path, int(i)}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}
