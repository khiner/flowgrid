#include "imgui_windows.h"
#include "imgui.h"

void ImGuiWindows::Metrics::draw(Window &window) {
    ImGui::ShowMetricsWindow(&window.visible);
}

void ImGuiWindows::Demo::draw(Window &window) {
    ImGui::ShowDemoWindow(&window.visible);
}

void ImGuiWindows::StyleEditor::draw(Window &) {
    ImGui::ShowStyleEditor();
}
