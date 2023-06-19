#include "Bool.h"

#include "imgui.h"

#include "UI/HelpMarker.h"

void Bool::Toggle() const { Action::Primitive::Bool::Toggle{Path}.q(); }

void Bool::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::Bool::Toggle &) { Set(!Get()); },
    );
}

using namespace ImGui;

void Bool::Render() const {
    bool value = Value;
    if (Checkbox(ImGuiLabel.c_str(), &value)) Toggle();
    HelpMarker();
}
bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) Toggle();
    HelpMarker();
    return toggled;
}
void Bool::MenuItem() const {
    const bool value = Value;
    HelpMarker(false);
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) Toggle();
}
