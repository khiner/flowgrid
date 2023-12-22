#include "TextBuffer.h"

#include "imgui.h"

#include "Core/Windows.h"
#include "Project/Audio/Faust/FaustAction.h"
#include "Project/TextEditor/TextEditor.h"
#include "UI/Fonts.h"

static const Menu FileMenu = {"File", {Action::Faust::File::ShowOpenDialog::MenuItem, Action::Faust::File::ShowSaveDialog::MenuItem}};

TextBuffer::TextBuffer(ComponentArgs &&args, string_view value)
    : Primitive(std::move(args), string(value)), Editor(std::make_unique<TextEditor>()) {
    Editor->SetLanguageDefinition(TextEditor::LanguageDefinitionIdT::Cpp);
}

TextBuffer::~TextBuffer() {}

void TextBuffer::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::TextBuffer::Set &a) { Set(a.value); },
    );
}

using namespace ImGui;

void TextBuffer::RenderMenu() const {
    auto &editor = *Editor;
    if (BeginMenuBar()) {
        FileMenu.Draw();
        if (BeginMenu("Edit")) {
            MenuItem("Read-only mode", nullptr, &editor.ReadOnly);
            Separator();
            if (MenuItem("Undo", "ALT-Backspace", nullptr, !editor.ReadOnly && editor.CanUndo())) editor.Undo();
            if (MenuItem("Redo", "Ctrl-Y", nullptr, !editor.ReadOnly && editor.CanRedo())) editor.Redo();
            Separator();
            if (MenuItem("Copy", "Ctrl-C", nullptr, editor.AnyCursorHasSelection())) editor.Copy();
            if (MenuItem("Cut", "Ctrl-X", nullptr, !editor.ReadOnly && editor.AnyCursorHasSelection())) editor.Cut();
            if (MenuItem("Paste", "Ctrl-V", nullptr, !editor.ReadOnly && GetClipboardText() != nullptr)) editor.Paste();
            Separator();
            if (MenuItem("Select all", nullptr, nullptr)) editor.SelectAll();
            EndMenu();
        }

        if (BeginMenu("View")) {
            if (BeginMenu("Palette")) {
                if (MenuItem("Mariana palette")) editor.SetPalette(TextEditor::PaletteIdT::Mariana);
                if (MenuItem("Dark palette")) editor.SetPalette(TextEditor::PaletteIdT::Dark);
                if (MenuItem("Light palette")) editor.SetPalette(TextEditor::PaletteIdT::Light);
                if (MenuItem("Retro blue palette")) editor.SetPalette(TextEditor::PaletteIdT::RetroBlue);
                EndMenu();
            }
            gWindows.ToggleDebugMenuItem(Debug);
            EndMenu();
        }
        EndMenuBar();
    }
}

void TextBuffer::Render() const {
    RenderMenu();

    auto &editor = *Editor;
    const auto [cursor_line, cursor_column] = editor.GetCursorLineColumn();
    const string editing_file = "no file";
    Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursor_line + 1, cursor_column + 1, editor.GetLineCount(),
        editor.Overwrite ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageDefinitionName(),
        editing_file.c_str()
    );

    const string prev_text = editor.GetText();
    PushFont(gFonts.FixedWidth);
    editor.Render("TextEditor");
    PopFont();

    // TODO this is not the usual immediate-mode case. Only set text if the text changed.
    //   This strategy of computing two full copies of the text is only temporary.
    //   Soon I'm incorporating the TextEditor state/undo/redo system into the FlowGrid system.
    const string new_text = editor.GetText();
    if (new_text != prev_text) {
        IssueSet(new_text);
    } else if (Value != new_text) {
        editor.SetText(Value);
    }
}

void TextBuffer::RenderDebug() const {
    Editor->DebugPanel();
}
