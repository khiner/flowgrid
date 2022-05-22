#include "../state.h"

void State::Metrics::draw() {
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
