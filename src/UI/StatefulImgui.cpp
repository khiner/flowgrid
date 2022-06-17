#include "StatefulImGui.h"
#include "../Context.h"

void StatefulImGui::DrawWindow(Window &window, ImGuiWindowFlags flags) {
    const auto &name = window.name;
    if (s.named(name).visible != window.visible) q(toggle_window{name});
    if (!window.visible) return;

    if (!ImGui::Begin(name.c_str(), &window.visible, flags)) {
        ImGui::End();
        return;
    }

    window.draw();

    ImGui::End();
}

void dock_window(const Window &w, ImGuiID node_id) {
    ImGui::DockBuilderDockWindow(w.name.c_str(), node_id);
}

void gestured() {
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivated()) c.end_gesture();
//    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture(); // This doesn't catch opening and closing a color edit without editing.
}

bool StatefulImGui::WindowToggleMenuItem(Window &w) {
    const auto &name = w.name;
    const bool edited = ImGui::MenuItem(name.c_str(), nullptr, w.visible);
    // The UI copy of the window state object is checked on every window draw,
    // and a `toggle_window` action is issued whenever the UI copy disagrees with the canonical `s` window state.
    // This allows for simply changing the UI copy variable, either in this toggle, or via the window close button,
    // or any other mechanism.
    if (edited) w.visible = !w.visible;
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

bool StatefulImGui::SliderInt(const char *label, int *v, int v_min, int v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    gestured();
    return edited;
}

bool StatefulImGui::SliderScalar(const char *label, ImGuiDataType data_type, void *p_data, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format, flags);
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

void StatefulImGui::MenuItem(ActionID action_id) {
    const char *menu_label = action::get_menu_label(action_id);
    const char *shortcut = action::shortcut_for_id.contains(action_id) ? action::shortcut_for_id.at(action_id).c_str() : nullptr;
    if (ImGui::MenuItem(menu_label, shortcut, false, c.action_allowed(action_id))) q(action::create(action_id));
}
