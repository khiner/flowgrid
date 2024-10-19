#include "Demo.h"

#include "imgui.h"
#include "implot.h"

Demo::Demo(ComponentArgs &&args, const ::FileDialog &dialog)
    : Component(std::move(args), ImGuiWindowFlags_MenuBar), Dialog(dialog) {}

void Demo::ImGuiDemo::Render() const {
    ImGui::ShowDemoWindow();
}

void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}

void Demo::Render() const {
    RenderTabs();
}
