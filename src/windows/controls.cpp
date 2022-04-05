#include "controls.h"
#include "../context.h"
#include "../stateful_imgui.h"

void Controls::draw(Window &) {
    ImGui::BeginDisabled(!c.can_undo());
    if (ImGui::Button("Undo")) { q.enqueue(undo{}); }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!c.can_redo());
    if (ImGui::Button("Redo")) { q.enqueue(redo{}); }
    ImGui::EndDisabled();

    window_toggle(ui_s.ui.windows.style_editor);
    window_toggle(ui_s.ui.windows.imgui.demo);
    window_toggle(ui_s.ui.windows.imgui.metrics);

    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }
}
