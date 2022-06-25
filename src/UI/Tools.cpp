#include "../State.h"

void State::Tools::draw() const {
    if (ImGui::BeginTabBar("##tools")) {
        if (ImGui::BeginTabItem("ImGui")) {
            if (ImGui::BeginTabBar("##imgui_tools")) {
                if (ImGui::BeginTabItem("Debug log")) {
                    ImGui::ShowDebugLog();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndTabItem();
        ImGui::EndTabBar();
    }
}
