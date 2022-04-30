#include "../windows.h"
#include "imgui.h"
#include "implot.h"

void ImGuiWindows::Demo::draw(Window &window) {
    ImGui::ShowDemoWindow(&window.visible);
}

void ImGuiWindows::ImPlotWindows::Demo::draw(Window &window) {
    ImPlot::ShowDemoWindow(&window.visible);
}

void ImGuiWindows::Metrics::draw(Window &) {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("ImGui")) {
            ImGui::ShowMetrics();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImPlot")) {
            ImPlot::ShowMetrics();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
