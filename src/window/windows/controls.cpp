#include "../windows.h"
#include "../../context.h"

void Controls::draw(Window &) {
    ImGui::BeginDisabled(!c.can_undo());
    if (ImGui::Button("Undo")) { q.enqueue(undo{}); }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!c.can_redo());
    if (ImGui::Button("Redo")) { q.enqueue(redo{}); }
    ImGui::EndDisabled();

    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }
}
