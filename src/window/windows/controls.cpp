#include "../windows.h"
#include "../../context.h"

void Controls::draw(Window &) {
    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }
}
