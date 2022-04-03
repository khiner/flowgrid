#include "controls.h"
#include "../context.h"

void window_toggle(const Window &w) {
    if (ImGui::Checkbox(w.name.c_str(), &ui_s.ui.window_named[w.name].visible)) { q.enqueue(toggle_window{w.name}); }
}

void Controls::draw(Window &) {
    ImGui::BeginDisabled(!c.can_undo());
    if (ImGui::Button("Undo")) { q.enqueue(undo{}); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!c.can_redo());
    if (ImGui::Button("Redo")) { q.enqueue(redo{}); }
    ImGui::EndDisabled();

    window_toggle(ui_s.ui.windows.imgui.demo);
    window_toggle(ui_s.ui.windows.imgui.metrics);
    window_toggle(ui_s.ui.windows.imgui.style_editor);

    if (ImGui::ColorEdit3("Background color", (float *) &ui_s.ui.colors.clear)) { q.enqueue(set_clear_color{ui_s.ui.colors.clear}); }
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture();

    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }
}
