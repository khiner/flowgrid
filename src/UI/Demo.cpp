#include "../State.h"
#include "../FileDialog/ImGuiFileDialogDemo.h"

void State::Demo::draw() {
    if (ImGui::BeginTabBar("##demos")) {
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
