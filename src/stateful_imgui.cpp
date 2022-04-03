#include "stateful_imgui.h"
#include "context.h"
#include "imgui_internal.h"

void dock_window(const Window &w, ImGuiID node_id) {
    ImGui::DockBuilderDockWindow(w.name.c_str(), node_id);
}

void window_toggle(const Window &w) {
    if (ImGui::Checkbox(w.name.c_str(), &ui_s.ui.window_named[w.name].visible)) { q.enqueue(toggle_window{w.name}); }
}

void gestured() {
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture();
}

bool StatefulImGui::ColorEdit3(const char *label, Color &color, ImGuiColorEditFlags flags) {
    const bool edited = ImGui::ColorEdit3(label, (float *) &color, flags);
    gestured();
    return edited;
}
