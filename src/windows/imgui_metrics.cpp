#include "imgui_metrics.h"
#include "imgui.h"

void ImGuiMetrics::draw(Window &window) {
    ImGui::ShowMetricsWindow(&window.visible);
}
