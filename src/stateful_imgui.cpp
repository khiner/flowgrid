#include "stateful_imgui.h"
#include "context.h"
#include "imgui_internal.h"

void dock_window(const std::string &name, ImGuiID node_id) {
    ImGui::DockBuilderDockWindow(name.c_str(), node_id);
}

void gestured() {
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture();
}

bool StatefulImGui::WindowToggleMenuItem(const std::string &name) {
    const bool edited = ImGui::MenuItem(name.c_str(), nullptr, &ui_s.ui.windows.named(name).visible);
    if (edited) q.enqueue(toggle_window{name});
    return edited;
}

bool StatefulImGui::SliderFloat(const char *label, float *v, float v_min, float v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
    gestured();
    return edited;
}

bool StatefulImGui::SliderFloat2(const char *label, float *v, float v_min, float v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
    gestured();
    return edited;
}

bool StatefulImGui::DragFloat(const char *label, float *v, float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
    gestured();
    return edited;
}

bool StatefulImGui::ColorEdit4(const char *label, float col[4], ImGuiColorEditFlags flags) {
    const bool edited = ImGui::ColorEdit4(label, col, flags);
    gestured();
    return edited;
}
