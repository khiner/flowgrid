#include "Bool.h"

#include "imgui.h"

#include "PrimitiveActionQueuer.h"
#include "UI/HelpMarker.h"

void Bool::IssueToggle() const { PrimitiveQ(Action::Primitive::Bool::Toggle{Id}); }

using namespace ImGui;

void Bool::Render(std::string_view label) const {
    if (bool value = Value; Checkbox(label.data(), &value)) IssueToggle();
    HelpMarker();
}

void Bool::Render() const {
    Render(ImGuiLabel);
}

bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) IssueToggle();
    HelpMarker();
    return toggled;
}

void Bool::MenuItem() const {
    HelpMarker(false);
    if (const bool value = Value; ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) IssueToggle();
}
