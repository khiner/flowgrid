#include "Menu.h"
#include "imgui.h"

void HelpMarker(const char *desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool BeginMenuWithHelp(const char *label, const char *help, bool enabled) {
    HelpMarker(help);
    ImGui::SameLine();
    return ImGui::BeginMenu(label, enabled);
}

bool MenuItemWithHelp(const char *label, const char *help, const char *shortcut, bool selected, bool enabled) {
    HelpMarker(help);
    ImGui::SameLine();
    return ImGui::MenuItem(label, shortcut, selected, enabled);
}
