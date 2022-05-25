#include "../action.h"
#include "../imgui_helpers.h"

namespace FlowGrid {

void ShowMetrics() {
    ImGui::Text("Action variant size: %lu bytes", sizeof(Action));
    ImGui::SameLine();
    HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
               "Thus, it's important to keep action data small.");
}

}

void State::Metrics::draw() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("FlowGrid")) {
            FlowGrid::ShowMetrics();
            ImGui::EndTabItem();
        }
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
