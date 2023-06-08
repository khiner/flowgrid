#include "Bool.h"

#include "imgui.h"

#include "UI/HelpMarker.h"

void Bool::Toggle() const { Action::Primitive::ToggleBool{Path}.q(); }

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
