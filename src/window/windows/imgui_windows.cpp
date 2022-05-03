#include "../../state.h"

#include "imgui.h"
#include "implot.h"

void Windows::Demos::draw() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("ImGui")) {
            ImGui::ShowDemo();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImPlot")) {
            ImPlot::ShowDemo();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void Windows::Metrics::draw() {
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
