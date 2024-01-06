#include "TextBuffer.h"

#include "imgui.h"

#include "Core/Windows.h"
#include "Helper/File.h"
#include "Project/FileDialog/FileDialog.h"
#include "Project/TextEditor/TextEditor.h"
#include "UI/Fonts.h"

TextBuffer::TextBuffer(ArgsT &&args, const ::FileDialog &file_dialog, TextBuffer::FileConfig &&file_config, string_view value)
    : Primitive(std::move(args.Args), string(value)), ActionableProducer(std::move(args.Q)),
      FileDialog(file_dialog), FileConf(std::move(file_config)), Editor(std::make_unique<TextEditor>()) {
    Editor->SetLanguageDefinition(TextEditor::LanguageDefinitionIdT::Cpp);
}
TextBuffer::TextBuffer(ArgsT &&args, const ::FileDialog &file_dialog, string_view value)
    : Primitive(std::move(args.Args), string(value)), ActionableProducer(std::move(args.Q)),
      FileDialog(file_dialog),
      FileConf({
          {
              .owner_path = Path,
              .title = "Open file",
              .filters = ".json",
          },
          {
              .owner_path = Path,
              .title = "Save file",
              .filters = ".json",
              .default_file_name = "my_json",
              .save_mode = true,
          },
      }),
      Editor(std::make_unique<TextEditor>()) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
    Editor->SetLanguageDefinition(TextEditor::LanguageDefinitionIdT::Cpp);
}

TextBuffer::~TextBuffer() {}

void TextBuffer::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::TextBuffer::Set &a) { Set(a.value); },
        [this](const Action::TextBuffer::ShowOpenDialog &) { FileDialog.Set(FileConf.OpenConfig); },
        [this](const Action::TextBuffer::ShowSaveDialog &) { FileDialog.Set(FileConf.SaveConfig); },
        [this](const Action::TextBuffer::Open &a) { Set(FileIO::read(a.file_path)); },
        [this](const Action::TextBuffer::Save &a) { FileIO::write(a.file_path, Editor->GetText()); },
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
            if (MenuItem("Paste", "Ctrl-V", nullptr, !editor.ReadOnly && ImGui::GetClipboardText() != nullptr)) editor.Paste();
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
    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath &&
        (FileDialog.OwnerPath == FileConf.OpenConfig.owner_path || FileDialog.OwnerPath == FileConf.SaveConfig.owner_path)) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        PrevSelectedPath = FileDialog.SelectedFilePath = "";
        if (FileDialog.SaveMode) Q(Action::TextBuffer::Save{Path, selected_path});
        else Q(Action::TextBuffer::Open{Path, selected_path});
    }

    RenderMenu();

    auto &editor = *Editor;
    const auto cursor_coords = editor.GetCursorPosition();
    const string editing_file = "no file";
    Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursor_coords.L + 1, cursor_coords.C + 1, editor.GetLineCount(),
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
