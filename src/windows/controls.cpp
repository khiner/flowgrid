#include "controls.h"
#include "../context.h"

struct InputTextCallback_UserData {
    std::string *Str;
    ImGuiInputTextCallback ChainCallback;
    void *ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    auto *user_data = (InputTextCallback_UserData *) data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        std::string *str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char *) str->c_str();
    } else if (user_data->ChainCallback) {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

bool InputTextMultiline(const char *label, std::string *str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data{str, callback, user_data};
    return ImGui::InputTextMultiline(label, (char *) str->c_str(), str->capacity() + 1, ImVec2(0, 0), flags, InputTextCallback, &cb_user_data);
}

void window_toggle(const std::string &name) {
    if (ImGui::Checkbox(name.c_str(), &ui_s.ui.windows[name].visible)) { q.enqueue(toggle_window{name}); }
}

void Controls::draw() {
    ImGui::BeginDisabled(!c.can_undo());
    if (ImGui::Button("Undo")) { q.enqueue(undo{}); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!c.can_redo());
    if (ImGui::Button("Redo")) { q.enqueue(redo{}); }
    ImGui::EndDisabled();
    window_toggle(WindowNames::imgui_demo);
    window_toggle(WindowNames::imgui_metrics);

    if (ImGui::ColorEdit3("Background color", (float *) &ui_s.ui.colors.clear)) { q.enqueue(set_clear_color{ui_s.ui.colors.clear}); }
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture();

    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
// TODO allow toggling action_consumer on and off repeatedly
//        q.enqueue(set_action_consumer_running{false});
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }

    {
//        ImGuiInputTextFlags_NoUndoRedo;
        static auto flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue;
        if (InputTextMultiline("##faust_source", &ui_s.audio.faust.code, flags)) {
            q.enqueue(set_faust_text{ui_s.audio.faust.code});
        }
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
        ImGui::PopStyleColor();
    }
}
