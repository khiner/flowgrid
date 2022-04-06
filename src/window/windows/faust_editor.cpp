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
    ZepWrapper(const fs::path &root_path, const Zep::NVec2f &pixel_scale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> callback)
        : editor(Zep::ZepPath(root_path.string()), pixel_scale), callback(std::move(callback)) {
        editor.RegisterCallback(this);
    }

    Zep::ZepEditor &GetEditor() const override { return (Zep::ZepEditor &) editor; }

    void Notify(std::shared_ptr<Zep::ZepMessage> message) override { callback(message); }

    Zep::ZepEditor_ImGui editor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> callback;
};

std::unique_ptr<ZepWrapper> zep;

void zep_init() {
    const Zep::NVec2f pixelScale(1.0f, 1.0f);
    zep = std::make_unique<ZepWrapper>(
        config.app_root,
        Zep::NVec2f(pixelScale.x, pixelScale.y),
        [](const std::shared_ptr<ZepMessage> &message) -> void {
            if (message->messageId == Msg::Buffer) {
                auto buffer_message = std::static_pointer_cast<BufferMessage>(message);
                switch (buffer_message->type) {
                    case BufferMessageType::TextChanged:
                    case BufferMessageType::TextDeleted:
                    case BufferMessageType::TextAdded: q.enqueue(set_faust_text{buffer_message->pBuffer->GetWorkingBuffer().string()});
                        break;
                    case BufferMessageType::PreBufferChange:
                    case BufferMessageType::Loaded:
                    case BufferMessageType::MarkersChanged: break;
                }
            }
        }
    );

    auto &display = zep->editor.GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.125)));
    zep->editor.InitWithText("Faust", ui_s.audio.faust.code);
}

bool zep_initialized = false;

NVec2f topLeft;
NVec2f bottomRight;

void zep_draw() {
    if (!zep_initialized) {
        // Called once after the fonts are initialized
        zep_init();
        zep_initialized = true;
    }

    const auto &pos = ImGui::GetWindowPos();
    const auto &top_left = ImGui::GetWindowContentRegionMin();
    const auto &bottom_right = ImGui::GetWindowContentRegionMax();
    const auto height = 200;
    topLeft = Zep::NVec2f(top_left.x + pos.x, top_left.y + pos.y);
    bottomRight = Zep::NVec2f(bottom_right.x + pos.x, top_left.y + pos.y + height);
    zep->editor.SetDisplayRegion(topLeft, bottomRight);
    zep->editor.Display();
    if (ImGui::IsWindowFocused()) zep->editor.HandleInput();
    else zep->editor.ResetCursorTimer();

    // TODO this is not the usual immediate-mode case. Only set text if an undo/redo has changed the text
    //  Really what I want is for an application undo/redo containing code text changes to do exactly what
    //  zep does for undo/redo internally.
//    if (false) zep->editor.GetBuffers()[0]->SetText(ui_s.audio.faust.code);
}


// Simple text editor
struct InputTextCallback_UserData {
    std::string *Str;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    auto *user_data = (InputTextCallback_UserData *) data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string *str = user_data->Str;
        str->resize(data->BufTextLen);
        data->Buf = (char *) str->c_str();
    }
    return 0;
}

void simple_draw() {
//        ImGuiInputTextFlags_NoUndoRedo;
    static auto flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize;
    std::string *str = &ui_s.audio.faust.code;
    InputTextCallback_UserData cb_user_data{str};
    if (ImGui::InputTextMultiline("##faust_source", (char *) str->c_str(), str->capacity() + 1, ImVec2(0, 200), flags, InputTextCallback, &cb_user_data)) {
        q.enqueue(set_faust_text{ui_s.audio.faust.code});
    }
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
    ImGui::SetCursorPosY(bottomRight.y);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
    ImGui::PopStyleColor();
}

void FaustEditor::destroy() {
    zep.reset();
}
