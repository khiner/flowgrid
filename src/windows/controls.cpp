#include "../context.h"

void Audio::Settings::draw() {
    if (ImGui::Checkbox("Audio thread running", &ui_s.processes.audio.running)) { q(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.settings.muted)) { q(toggle_audio_muted{}); }
}
