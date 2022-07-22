#include "Widgets.h"
#include "../Context.h"

void fg::DrawWindow(const Window &window, ImGuiWindowFlags flags) {
    if (!window.visible) return;

    bool visible = window.visible;
    if (ImGui::Begin(window.name.c_str(), &visible, flags)) {
        if (visible) window.draw();
        else q(close_window{window.name});
    }
    ImGui::End();
}

void dock_window(const Window &window, ImGuiID node_id) {
    ImGui::DockBuilderDockWindow(window.name.c_str(), node_id);
}

void gestured() {
    if (ImGui::IsItemActivated()) c.gesturing = true;
    if (ImGui::IsItemDeactivated()) c.gesturing = false;
}

bool fg::WindowToggleMenuItem(const Window &window) {
    const bool edited = ImGui::MenuItem(window.name.c_str(), nullptr, window.visible);
    if (edited) q(toggle_window{window.name});
    return edited;
}

bool fg::Checkbox(const char *path, const char *label) {
    bool v = sj[JsonPath(path)];
    const bool edited = ImGui::Checkbox(label ? label : path_label(path).c_str(), &v);
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::SliderFloat(const char *path, float v_min, float v_max, const char *format, ImGuiSliderFlags flags, const char *label) {
    float v = sj[JsonPath(path)];
    const bool edited = ImGui::SliderFloat(label ? label : path_label(path).c_str(), &v, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::SliderFloat2(const char *path, float v_min, float v_max, const char *format, ImGuiSliderFlags flags) {
    ImVec2 v = sj[JsonPath(path)];
    const bool edited = ImGui::SliderFloat2(path_label(path).c_str(), (float *) &v, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::SliderInt(const char *label, int *v, int v_min, int v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    gestured();
    return edited;
}

bool fg::DragFloat(const char *path, float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags, const char *label) {
    float v = sj[JsonPath(path)];
    const bool edited = ImGui::DragFloat(label ? label : path_label(path).c_str(), &v, v_speed, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::ColorEdit4(const char *path, ImGuiColorEditFlags flags, const char *label) {
    ImVec4 v = sj[JsonPath(path)];
    const bool edited = ImGui::ColorEdit4(label ? label : path_label(path).c_str(), (float *) &v, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

void fg::MenuItem(ActionID action_id) {
    const char *menu_label = action::get_menu_label(action_id);
    const char *shortcut = action::shortcut_for_id.contains(action_id) ? action::shortcut_for_id.at(action_id).c_str() : nullptr;
    if (ImGui::MenuItem(menu_label, shortcut, false, c.action_allowed(action_id))) q(action::create(action_id));
}

bool fg::Combo(const char *path, const char *items_separated_by_zeros, int popup_max_height_in_items) {
    int v = sj[JsonPath(path)];
    const bool edited = ImGui::Combo(path_label(path).c_str(), &v, items_separated_by_zeros, popup_max_height_in_items);
    if (edited) q(set_value{path, v});
    return edited;
}
