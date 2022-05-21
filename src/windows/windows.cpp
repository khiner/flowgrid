#include "../stateful_imgui.h"

void Windows::draw() {
    StatefulImGui::DrawWindow(style_editor);
    StatefulImGui::DrawWindow(demos, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(metrics);
}
