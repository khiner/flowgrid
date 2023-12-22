#include "Bool.h"

#include "imgui.h"

#include "UI/HelpMarker.h"
#include "PrimitiveActionQueuer.h"

void Bool::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::Bool::Toggle &) { Set(!Get()); },
    );
}

void Bool::IssueToggle() const { PrimitiveQ.QueueToggle(Path); }

using namespace ImGui;

void Bool::Render(string_view label) const {
    bool value = Value;
    if (Checkbox(string(label).c_str(), &value)) IssueToggle();
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
    const bool value = Value;
    HelpMarker(false);
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) IssueToggle();
}
