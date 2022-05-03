#include "stateful_imgui.h"
#include "context.h"
#include "imgui_internal.h"

void dock_window(const std::string &name, ImGuiID node_id) {
    ImGui::DockBuilderDockWindow(name.c_str(), node_id);
}

// TODO move `wrap_draw_in_window` into a new `StatefulImGuiWindowFlags : ImGuiWindowFlags` type
void draw_window(const std::string &name, Window &window, ImGuiWindowFlags flags, bool wrap_draw_in_window) {
    const auto &w = s.ui.windows.named(name);
    auto &_w = ui_s.ui.windows.named(name);
    if (w.visible != _w.visible) q.enqueue(toggle_window{_w.name});
    if (!_w.visible) return;

    if (wrap_draw_in_window) {
        if (!ImGui::Begin(w.name.c_str(), &_w.visible, flags)) {
            ImGui::End();
            return;
        }
    } else {
        if (!_w.visible) return;
    }

    window.draw();

    if (wrap_draw_in_window) ImGui::End();
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
