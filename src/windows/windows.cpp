#include "../stateful_imgui.h"

void Windows::draw() {
    StatefulImGui::DrawWindow(demos, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(metrics);
}
