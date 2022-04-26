#include "../windows.h"
#include "imgui.h"
#include "implot.h"

void ImGuiWindows::Metrics::draw(Window &window) {
    ImGui::ShowMetricsWindow(&window.visible);
}

void ImGuiWindows::Demo::draw(Window &window) {
    ImGui::ShowDemoWindow(&window.visible);
}

void ImGuiWindows::ImPlotWindows::Demo::draw(Window &window) {
    ImPlot::ShowDemoWindow(&window.visible);
}

void ImGuiWindows::ImPlotWindows::Metrics::draw(Window &window) {
    ImPlot::ShowMetricsWindow(&window.visible);
}
