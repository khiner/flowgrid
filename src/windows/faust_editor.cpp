/**
 * Based on https://github.com/cmaughan/zep_imgui/blob/main/demo/src/editor.cpp
 */

#include "faust_editor.h"
#include "../config.h"
#include "../context.h"

using namespace Zep;

// Initialize the editor and watch for changes
void FaustEditor::zep_init(const Zep::NVec2f &pixelScale) {
    spZep = std::make_unique<ZepWrapper>(
        config.app_root,
        Zep::NVec2f(pixelScale.x, pixelScale.y),
        [](const std::shared_ptr<ZepMessage> &_) -> void {}
    );

    auto &display = spZep->GetEditor().GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.125)));
}

void FaustEditor::zep_load(const Zep::ZepPath &file) {
    auto pBuffer = spZep->GetEditor().InitWithFileOrDir(file);
}

// Simple text editor
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
// End simple text editor

void FaustEditor::zep_draw() {
    if (!initialized) {
        // Called once after the fonts are initialized
        zep_init(Zep::NVec2f(1.0f, 1.0f));
        zep_load(Zep::ZepPath(config.app_root) / "src" / "main.cpp");
        initialized = true;
    }

    spZep->GetEditor().RefreshRequired(); // Required for CTRL+P and flashing cursor.

    const auto &vMin = ImGui::GetWindowContentRegionMin();
    const auto &vMax = ImGui::GetWindowContentRegionMax();
    const auto &pos = ImGui::GetWindowPos();
    spZep->zepEditor.SetDisplayRegion(Zep::NVec2f(vMin.x + pos.x, vMin.y + pos.y), Zep::NVec2f(vMax.x + pos.x, vMax.y + pos.y));
    spZep->zepEditor.Display();
    if (ImGui::IsWindowFocused()) spZep->zepEditor.HandleInput();
}

void imgui_draw() {
//        ImGuiInputTextFlags_NoUndoRedo;
    static auto flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue;
    if (InputTextMultiline("##faust_source", &ui_s.audio.faust.code, flags)) {
        q.enqueue(set_faust_text{ui_s.audio.faust.code});
    }
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
    ImGui::PopStyleColor();
}

void FaustEditor::draw(Window &) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Simple text editor", nullptr, &ui_s.audio.faust.simple_text_editor)) { q.enqueue(toggle_faust_simple_text_editor{}); }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (s.audio.faust.simple_text_editor) imgui_draw();
    else zep_draw();
}

void FaustEditor::destroy() {
    spZep.reset();
}
