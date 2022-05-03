#include "../windows.h"
#include "../../context.h"

#include "ImGuiFileDialog.h"

#include "zep/editor.h"
#include "zep/regress.h"
#include "zep/mode_repl.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/tab_window.h"

using namespace Zep;

inline NVec2f toNVec2f(const ImVec2 &im) { return {im.x, im.y}; }
inline ImVec2 toImVec2(const NVec2f &im) { return {im.x, im.y}; }

struct ZepFont_ImGui : public ZepFont {
    ZepFont_ImGui(ZepDisplay &display, ImFont *font, float heightRatio) : ZepFont(display), font(font) {
        SetPixelHeight(int(font->FontSize * heightRatio));
    }

    void SetPixelHeight(int pixelHeight) override {
        InvalidateCharCache();
        this->pixelHeight = pixelHeight;
    }

    NVec2f GetTextSize(const uint8_t *begin, const uint8_t *end) const override {
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        ImVec2 text_size = font->CalcTextSizeA(float(pixelHeight), FLT_MAX, FLT_MAX, (const char *) begin, (const char *) end, nullptr);
        if (text_size.x == 0.0) {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_size = font->CalcTextSizeA(float(pixelHeight), FLT_MAX, FLT_MAX, &chDefault, (&chDefault + 1), nullptr);
        }

        return toNVec2f(text_size);
    }

    ImFont *font;
};

static ImU32 GetStyleModulatedColor(const NVec4f &color) {
    return ToPackedABGR(NVec4f(color.x, color.y, color.z, color.w * ImGui::GetStyle().Alpha));
}

struct ZepDisplay_ImGui : public ZepDisplay {
    ZepDisplay_ImGui() : ZepDisplay() {}

    void DrawChars(ZepFont &font, const NVec2f &pos, const NVec4f &col, const uint8_t *text_begin, const uint8_t *text_end) const override {
        auto imFont = dynamic_cast<ZepFont_ImGui &>(font).font;
        auto *drawList = ImGui::GetWindowDrawList();
        if (text_end == nullptr) {
            text_end = text_begin + strlen((const char *) text_begin);
        }
        const auto modulatedColor = GetStyleModulatedColor(col);
        if (clipRect.Width() == 0) {
            drawList->AddText(imFont, float(font.pixelHeight), toImVec2(pos), modulatedColor, (const char *) text_begin, (const char *) text_end);
        } else {
            drawList->PushClipRect(toImVec2(clipRect.topLeftPx), toImVec2(clipRect.bottomRightPx));
            drawList->AddText(imFont, float(font.pixelHeight), toImVec2(pos), modulatedColor, (const char *) text_begin, (const char *) text_end);
            drawList->PopClipRect();
        }
    }

    void DrawLine(const NVec2f &start, const NVec2f &end, const NVec4f &color, float width) const override {
        auto *drawList = ImGui::GetWindowDrawList();
        const auto modulatedColor = GetStyleModulatedColor(color);

        // Background rect for numbers
        if (clipRect.Width() == 0) {
            drawList->AddLine(toImVec2(start), toImVec2(end), modulatedColor, width);
        } else {
            drawList->PushClipRect(toImVec2(clipRect.topLeftPx), toImVec2(clipRect.bottomRightPx));
            drawList->AddLine(toImVec2(start), toImVec2(end), modulatedColor, width);
            drawList->PopClipRect();
        }
    }

    void DrawRectFilled(const NRectf &rc, const NVec4f &color) const override {
        auto *drawList = ImGui::GetWindowDrawList();
        const auto modulatedColor = GetStyleModulatedColor(color);
        // Background rect for numbers
        if (clipRect.Width() == 0) {
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), modulatedColor);
        } else {
            drawList->PushClipRect(toImVec2(clipRect.topLeftPx), toImVec2(clipRect.bottomRightPx));
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), modulatedColor);
            drawList->PopClipRect();
        }
    }

    void SetClipRect(const NRectf &rc) override { clipRect = rc; }

    ZepFont &GetFont(ZepTextType type) override {
        if (fonts[(int) type] == nullptr) {
            fonts[(int) type] = std::make_shared<ZepFont_ImGui>(*this, ImGui::GetIO().Fonts[0].Fonts[0], int(16.0f * pixelScale.y));
        }
        return *fonts[(int) type];
    }

    NRectf clipRect;
};

struct ZepEditor_ImGui : public ZepEditor {
    explicit ZepEditor_ImGui(const ZepPath &root, uint32_t flags = 0, ZepFileSystem *pFileSystem = nullptr)
        : ZepEditor(new ZepDisplay_ImGui(), root, flags, pFileSystem) {}

    bool sendImGuiKeyPressToBuffer(ImGuiKey key, ImGuiKeyModFlags mod = ImGuiKeyModFlags_None) {
        if (ImGui::IsKeyPressed(key)) {
            activeTabWindow->GetActiveWindow()->buffer->GetMode()->AddKeyPress(key, mod);
            return true;
        }
        return false;
    }

    void handleMouseEventAndHideFromImGui(size_t mouseButtonIndex, ZepMouseButton zepMouseButton, bool down) {
        auto &io = ImGui::GetIO();
        if (down) {
            if (io.MouseClicked[mouseButtonIndex] && OnMouseDown(toNVec2f(io.MousePos), zepMouseButton)) io.MouseClicked[mouseButtonIndex] = false;
        }
        if (io.MouseReleased[mouseButtonIndex] && OnMouseUp(toNVec2f(io.MousePos), zepMouseButton)) io.MouseReleased[mouseButtonIndex] = false;
    }

    void HandleInput() override {
        auto &io = ImGui::GetIO();
        bool handled = false;
        ImGuiKeyModFlags mod = 0;

        static std::vector<ImGuiKey> F_KEYS = {
            ImGuiKey_F1,
            ImGuiKey_F2,
            ImGuiKey_F3,
            ImGuiKey_F4,
            ImGuiKey_F5,
            ImGuiKey_F6,
            ImGuiKey_F7,
            ImGuiKey_F8,
            ImGuiKey_F9,
            ImGuiKey_F10,
            ImGuiKey_F11,
            ImGuiKey_F12,
        };

        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
            OnMouseMove(toNVec2f(io.MousePos));
        }

        handleMouseEventAndHideFromImGui(0, ZepMouseButton::Left, true);
        handleMouseEventAndHideFromImGui(1, ZepMouseButton::Right, true);
        handleMouseEventAndHideFromImGui(0, ZepMouseButton::Left, false);
        handleMouseEventAndHideFromImGui(1, ZepMouseButton::Right, false);

        if (io.KeyCtrl) mod |= ImGuiKeyModFlags_Ctrl;
        if (io.KeyShift) mod |= ImGuiKeyModFlags_Shift;

        const auto *buffer = activeTabWindow->GetActiveWindow()->buffer;

        // Check USB Keys
        for (auto &f_key: F_KEYS) {
            if (ImGui::IsKeyPressed(f_key)) {
                buffer->GetMode()->AddKeyPress(f_key, mod);
                return;
            }
        }

        if (sendImGuiKeyPressToBuffer(ImGuiKey_Tab, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_Escape, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_Enter, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_Delete, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_Home, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_End, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_Backspace, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_RightArrow, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_LeftArrow, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_UpArrow, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_DownArrow, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_PageDown, mod)) return;
        if (sendImGuiKeyPressToBuffer(ImGuiKey_PageUp, mod)) return;

        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            } else {
                for (ImGuiKey key = ImGuiKey_A; key < ImGuiKey_Z; key++) {
                    if (ImGui::IsKeyPressed(key)) {
                        buffer->GetMode()->AddKeyPress(key, mod);
                        handled = true;
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
                    buffer->GetMode()->AddKeyPress(ImGuiKey_Space, mod);
                    handled = true;
                }
            }
        }

        if (!handled) {
            for (ImWchar ch: io.InputQueueCharacters) {
                if (ch == '\r') continue; // Ignore '\r' - sometimes ImGui generates it!
                ImGuiKey key = ch + ImGuiKey_A - 'a';
                buffer->GetMode()->AddKeyPress(key, mod);
            }
        }
    }
};

bool zep_initialized = false;

struct ZepWrapper : public ZepComponent, public IZepReplProvider {
    explicit ZepWrapper(ZepEditor_ImGui &editor) : ZepComponent(editor) {
        ZepRegressExCommand::Register(editor);

        // Repl
        ZepReplExCommand::Register(editor, this);
        ZepReplEvaluateOuterCommand::Register(editor, this);
        ZepReplEvaluateInnerCommand::Register(editor, this);
        ZepReplEvaluateCommand::Register(editor, this);
    }

    void Notify(const std::shared_ptr<ZepMessage> &message) override {
        if (message->messageId == Msg::Buffer) {
            auto buffer_message = std::static_pointer_cast<BufferMessage>(message);
            switch (buffer_message->type) {
                case BufferMessageType::TextChanged:
                case BufferMessageType::TextDeleted:
                case BufferMessageType::TextAdded: {
                    auto *buffer = buffer_message->buffer;
                    if (zep_initialized && buffer->name == s.audio.faust.editor.file_name) {
                        // Redundant `c_str()` call removes an extra null char that seems to be at the end of the buffer string
                        q.enqueue(set_faust_code{buffer->workingBuffer.string().c_str()}); // NOLINT(readability-redundant-string-cstr)
                    }
                }
                    break;
                case BufferMessageType::PreBufferChange:
                case BufferMessageType::Loaded:
                case BufferMessageType::MarkersChanged: break;
            }
        }
    }


    std::string ReplParse(ZepBuffer &buffer, const GlyphIterator &cursorOffset, ReplParseType type) override {
        ZEP_UNUSED(cursorOffset);
        ZEP_UNUSED(type);

        GlyphRange range = type == ReplParseType::OuterExpression ?
                           buffer.GetExpression(ExpressionType::Outer, cursorOffset, {'('}, {')'}) :
                           type == ReplParseType::SubExpression ?
                           buffer.GetExpression(ExpressionType::Inner, cursorOffset, {'('}, {')'}) :
                           GlyphRange(buffer.Begin(), buffer.End());

        if (range.first >= range.second) return "<No Expression>";

        // Flash the evaluated expression
        auto flashType = FlashType::Flash;
        float time = 1.0f;
        buffer.BeginFlash(time, flashType, range);

//        const auto &text = buffer.workingBuffer;
//        auto eval = std::string(text.begin() + range.first.index, text.begin() + range.second.index);
//        auto ret = chibi_repl(scheme, NULL, eval);
//        ret = RTrim(ret);
//
//        editor->SetCommandText(ret);
//        return ret;

        return "";
    }

    std::string ReplParse(const std::string &str) override {
//        auto ret = chibi_repl(scheme, NULL, str);
//        ret = RTrim(ret);
//        return ret;
        return str;
    }

    bool ReplIsFormComplete(const std::string &str, int &indent) override {
        int count = 0;
        for (auto &ch: str) {
            if (ch == '(') count++;
            if (ch == ')') count--;
        }

        if (count < 0) {
            indent = -1;
            return false;
        }

        if (count == 0) return true;

        int count2 = 0;
        indent = 1;
        for (auto &ch: str) {
            if (ch == '(') count2++;
            if (ch == ')') count2--;
            if (count2 == count) break;
            indent++;
        }
        return false;
    }

    std::function<void(std::shared_ptr<ZepMessage>)> callback;
};

std::unique_ptr<ZepWrapper> zep;
std::unique_ptr<ZepEditor_ImGui> editor;

void zep_init() {
    editor = std::make_unique<ZepEditor_ImGui>(ZepPath(config.app_root));
    zep = std::make_unique<ZepWrapper>(*editor);

    auto *display = editor->display;
    display->SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(*display, c.fixedWidthFont, 1.0));
    display->SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(*display, c.fixedWidthFont, 1.0));
    display->SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(*display, c.fixedWidthFont, 1.5));
    display->SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(*display, c.fixedWidthFont, 1.25));
    display->SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(*display, c.fixedWidthFont, 1.125));
    editor->InitWithText(s.audio.faust.editor.file_name, ui_s.audio.faust.code);
}

void zep_draw() {
    if (!zep_initialized) {
        // Called once after the fonts are initialized
        zep_init();
        zep_initialized = true;
    }

    const auto &pos = ImGui::GetWindowPos();
    const auto &top_left = ImGui::GetWindowContentRegionMin();
    const auto &bottom_right = ImGui::GetWindowContentRegionMax();
    editor->SetDisplayRegion({
        {top_left.x + pos.x,     top_left.y + pos.y},
        {bottom_right.x + pos.x, bottom_right.y + pos.y}
    });

    //    editor->RefreshRequired(); // TODO Save battery by skipping display if not required.
    editor->Display();
    if (ImGui::IsWindowFocused()) editor->HandleInput();
    else editor->ResetCursorTimer();

    // TODO this is not the usual immediate-mode case. Only set text if an undo/redo has changed the text
    //  Really what I want is for an application undo/redo containing code text changes to do exactly what
    //  zep does for undo/redo internally.
//    if (false) editor->buffers[0]->SetText(ui_s.audio.faust.code);
}

static const std::string openFileDialogKey = "ChooseFileDlgKey";

/*
 * TODO
 *   Implement `w` forward-word navigation for Vim mode
 *   Two-finger mouse pad scrolling
 *   Add mouse selection https://github.com/Rezonality/zep/issues/56
 *   Standard mode select-all left navigation moves cursor from the end of the selection, but should move from beginning
 *     (and right navigation should move from the end)
 */
void FaustEditor::draw() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open")) {
                ImGuiFileDialog::Instance()->OpenDialog(openFileDialogKey, "Choose file", ".cpp,.h,.hpp", ".");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::BeginMenu("Editor mode")) {
                const auto *buffer = editor->activeTabWindow->GetActiveWindow()->buffer;
                bool enabledVim = strcmp(buffer->GetMode()->Name(), ZepMode_Vim::StaticName()) == 0;
                bool enabledNormal = !enabledVim;
                if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim)) {
                    editor->SetGlobalMode(ZepMode_Vim::StaticName());
                } else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal)) {
                    editor->SetGlobalMode(ZepMode_Standard::StaticName());
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Theme")) {
                bool enabledDark = editor->theme->GetThemeType() == ThemeType::Dark ? true : false;
                bool enabledLight = !enabledDark;

                if (ImGui::MenuItem("Dark", "", &enabledDark)) {
                    editor->theme->SetThemeType(ThemeType::Dark);
                } else if (ImGui::MenuItem("Light", "", &enabledLight)) {
                    editor->theme->SetThemeType(ThemeType::Light);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            auto *tabWindow = editor->activeTabWindow;
            if (ImGui::MenuItem("Horizontal split")) {
                tabWindow->AddWindow(tabWindow->GetActiveWindow()->buffer, tabWindow->GetActiveWindow(), RegionLayoutType::VBox);
            } else if (ImGui::MenuItem("Vertical split")) {
                tabWindow->AddWindow(tabWindow->GetActiveWindow()->buffer, tabWindow->GetActiveWindow(), RegionLayoutType::HBox);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();

        if (ImGuiFileDialog::Instance()->Display(openFileDialogKey)) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                auto buffer = editor->GetFileBuffer(filePathName);
                editor->activeTabWindow->GetActiveWindow()->SetBuffer(buffer);
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }

    zep_draw();
}

void FaustEditor::destroy() {
    zep.reset();
}

void FaustLog::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
    ImGui::PopStyleColor();
}
