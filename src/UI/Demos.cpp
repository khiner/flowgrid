#include "../State.h"
#include "ImGuiFileDialog.h"
#include "../FileDialog/ImGuiFileDialogDemo.h"

void State::Demo::draw() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("ImGui")) {
            ImGui::ShowDemo();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImPlot")) {
            ImPlot::ShowDemo();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImGuiFileDialog")) {
            IGFD::ShowDemo();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
