#include "Demo.h"

#include "imgui.h"
#include "implot.h"

Demo::Demo(ArgsT &&args) : ActionProducerComponent(std::move(args)) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
}

void Demo::ImGuiDemo::Render() const {
    ImGui::ShowDemoWindow();
}

void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}

void Demo::Render() const {
    RenderTabs();
}
