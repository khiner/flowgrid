#include "Demo.h"

#include "imgui.h"
#include "implot.h"

Demo::Demo(Stateful *parent, string_view path_leaf, string_view meta_str)
    : TabsWindow(parent, path_leaf, meta_str, ImGuiWindowFlags_MenuBar) {}

void Demo::ImGuiDemo::Render() const {
    ImGui::ShowDemoWindow();
}

void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}
