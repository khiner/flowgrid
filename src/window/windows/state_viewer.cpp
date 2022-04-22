#include "../windows.h"
#include "../../context.h"

void StateViewer::draw(Window &) {
    std::string state_formatted = c.json_state_formatted;
    ImGui::TextUnformatted(&state_formatted.front(), &state_formatted.back());
//    ImGui::Text(formatted_state);
//    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
//    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }
}
