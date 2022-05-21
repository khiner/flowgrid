#include "../stateful_imgui.h"

void Windows::draw() {
    StatefulImGui::DrawWindow(state.memory_editor, ImGuiWindowFlags_NoScrollbar);
    StatefulImGui::DrawWindow(state.viewer, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(state.path_update_frequency, ImGuiWindowFlags_None);

    StatefulImGui::DrawWindow(style_editor);
    StatefulImGui::DrawWindow(demos, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(metrics);
}
