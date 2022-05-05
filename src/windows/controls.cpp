#include "../context.h"

void WindowsData::Controls::draw() {
    if (ImGui::Checkbox("Audio thread running", &ui_s.processes.audio.running)) { q(toggle_audio_running{}); }
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q(toggle_audio_muted{}); }
}
