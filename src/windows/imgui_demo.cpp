#include "imgui_demo.h"
#include "imgui.h"

void ImGuiDemo::draw(Window &window) {
    ImGui::ShowDemoWindow(&window.visible);
}
