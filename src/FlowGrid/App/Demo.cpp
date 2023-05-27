#include "Demo.h"

#include "imgui.h"
#include "implot.h"

Demo::Demo(StateMember *parent, string_view path_segment, string_view name_help)
    : TabsWindow(parent, path_segment, name_help, ImGuiWindowFlags_MenuBar) {}

void Demo::ImGuiDemo::Render() const {
    ImGui::ShowDemoWindow();
}

void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}
