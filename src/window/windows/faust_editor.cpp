/**
 * Based on https://github.com/cmaughan/zep_imgui/blob/main/demo/src/editor.cpp
 */

#include "../windows.h"

#include <filesystem>
#include "zep.h"
#include "../../config.h"
#include "../../context.h"

using namespace Zep;
namespace fs = std::filesystem;

struct ZepWrapper : public Zep::IZepComponent {
    ZepWrapper(const fs::path &root_path, const Zep::NVec2f &pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : editor(Zep::ZepPath(root_path.string()), pixelScale), Callback(std::move(fnCommandCB)) {
        editor.RegisterCallback(this);
    }

    Zep::ZepEditor &GetEditor() const override { return (Zep::ZepEditor &) editor; }

    void Notify(std::shared_ptr<Zep::ZepMessage> message) override { Callback(message); }

    virtual void HandleInput() { editor.HandleInput(); }

    Zep::ZepEditor_ImGui editor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

std::unique_ptr<ZepWrapper> zep;

void zep_init() {
    const Zep::NVec2f pixelScale(1.0f, 1.0f);
    zep = std::make_unique<ZepWrapper>(
        config.app_root,
        Zep::NVec2f(pixelScale.x, pixelScale.y),
        [](const std::shared_ptr<ZepMessage> &) -> void {}
    );

    auto &display = zep->editor.GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.125)));
    //    auto pBuffer = zep->editor.InitWithFileOrDir(file);
}

bool zep_initialized = false;

void zep_draw() {
    if (!zep_initialized) {
        // Called once after the fonts are initialized
        zep_init();
        zep_initialized = true;
    }

    zep->GetEditor().RefreshRequired(); // Required for CTRL+P and flashing cursor.

    const auto &pos = ImGui::GetWindowPos();
    const auto &top_left = ImGui::GetWindowContentRegionMin();
    const auto &bottom_right = ImGui::GetWindowContentRegionMax();
    zep->editor.SetDisplayRegion(
        Zep::NVec2f(top_left.x + pos.x, top_left.y + pos.y),
        Zep::NVec2f(bottom_right.x + pos.x, bottom_right.y + pos.y)
    );
    zep->editor.Display();
    if (ImGui::IsWindowFocused()) zep->editor.HandleInput();
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

void simple_draw() {
//        ImGuiInputTextFlags_NoUndoRedo;
    static auto flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue;
    if (InputTextMultiline("##faust_source", &ui_s.audio.faust.code, flags)) {
        q.enqueue(set_faust_text{ui_s.audio.faust.code});
    }
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
    ImGui::PopStyleColor();
}
// End simple text editor


void FaustEditor::draw(Window &) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Simple text editor", nullptr, &ui_s.audio.faust.simple_text_editor)) { q.enqueue(toggle_faust_simple_text_editor{}); }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (s.audio.faust.simple_text_editor) simple_draw();
    else zep_draw();
}

void FaustEditor::destroy() {
    zep.reset();
}
