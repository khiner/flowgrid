#include "TextEditor.h"

#include "imgui.h"

#include "Core/Windows.h"
#include "Helper/File.h"
#include "Project/FileDialog/FileDialog.h"
#include "Project/TextEditor/TextBuffer.h"
#include "UI/Fonts.h"

static TextEditor::FileConfig CreateDefaultFileConfig(const fs::path &path) {
    return {
        {
            .owner_path = path,
            .title = "Open file",
            .filters = TextBuffer::GetLanguageFileExtensionsFilter(),
        },
        {
            .owner_path = path,
            .title = "Save file",
            .filters = TextBuffer::GetLanguageFileExtensionsFilter(),
            .default_file_name = "my_json",
            .save_mode = true,
        },
    };
}

TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, TextEditor::FileConfig &&file_config, string_view text, LanguageID language_id)
    : ActionableComponent(std::move(args)), FileDialog(file_dialog), FileConf(std::move(file_config)), Buffer(std::make_unique<TextBuffer>(text, language_id)) {
    Text.Set_(string(text));
}
TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, string_view text, LanguageID language_id)
    : TextEditor(std::move(args), file_dialog, CreateDefaultFileConfig(Path), std::move(text), language_id) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
}
TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, const fs::path &file_path)
    : ActionableComponent(std::move(args)), FileDialog(file_dialog), FileConf(CreateDefaultFileConfig(Path)), Buffer(std::make_unique<TextBuffer>(file_path)) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
    Text.Set_(Buffer->GetText());
    LastOpenedFilePath.Set(file_path);
}

TextEditor::~TextEditor() {}

void TextEditor::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::TextEditor::Set &a) { Text.Set(a.value); },
            [this](const Action::TextEditor::ShowOpenDialog &) { FileDialog.Set(FileConf.OpenConfig); },
            [this](const Action::TextEditor::ShowSaveDialog &) { FileDialog.Set(FileConf.SaveConfig); },
            [this](const Action::TextEditor::Open &a) {
                LastOpenedFilePath.Set(a.file_path);
                Text.Set(FileIO::read(a.file_path));
            },
            [this](const Action::TextEditor::Save &a) { FileIO::write(a.file_path, Buffer->GetText()); },
        },
        action
    );
}

using namespace ImGui;

void TextEditor::RenderMenu() const {
    if (BeginMenuBar()) {
        auto &editor = *Buffer;
        FileMenu.Draw();
        if (BeginMenu("Edit")) {
            MenuItem("Read-only mode", nullptr, &editor.ReadOnly);
            Separator();
            if (MenuItem("Undo", "cmd+z", nullptr, !editor.ReadOnly && editor.CanUndo())) editor.Undo();
            if (MenuItem("Redo", "shift+cmd+z", nullptr, !editor.ReadOnly && editor.CanRedo())) editor.Redo();
            Separator();
            if (MenuItem("Copy", "cmd+c", nullptr, editor.CanCopy())) editor.Copy();
            if (MenuItem("Cut", "cmd+x", nullptr, editor.CanCut())) editor.Cut();
            if (MenuItem("Paste", "cmd+v", nullptr, editor.CanPaste())) editor.Paste();
            Separator();
            if (MenuItem("Select all", nullptr, nullptr)) editor.SelectAll();
            EndMenu();
        }

        if (BeginMenu("View")) {
            if (BeginMenu("Palette")) {
                if (MenuItem("Mariana palette")) editor.SetPalette(TextBuffer::PaletteIdT::Mariana);
                if (MenuItem("Dark palette")) editor.SetPalette(TextBuffer::PaletteIdT::Dark);
                if (MenuItem("Light palette")) editor.SetPalette(TextBuffer::PaletteIdT::Light);
                if (MenuItem("Retro blue palette")) editor.SetPalette(TextBuffer::PaletteIdT::RetroBlue);
                EndMenu();
            }
            MenuItem("Show style transition points", nullptr, &editor.ShowStyleTransitionPoints);
            MenuItem("Show changed capture ranges", nullptr, &editor.ShowChangedCaptureRanges);
            gWindows.ToggleDebugMenuItem(Debug);
            EndMenu();
        }
        EndMenuBar();
    }
}

void TextEditor::Render() const {
    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath &&
        (FileDialog.OwnerPath == FileConf.OpenConfig.owner_path || FileDialog.OwnerPath == FileConf.SaveConfig.owner_path)) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        PrevSelectedPath = FileDialog.SelectedFilePath = "";
        if (FileDialog.SaveMode) Q(Action::TextEditor::Save{Path, selected_path});
        else Q(Action::TextEditor::Open{Path, selected_path});
    }

    RenderMenu();

    auto &editor = *Buffer;
    const auto cursor_coords = editor.GetCursorPosition();
    const string editing_file = LastOpenedFilePath ? string(fs::path(LastOpenedFilePath).filename()) : "No file";
    ImGui::Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursor_coords.L + 1, cursor_coords.C + 1, editor.LineCount(),
        editor.Overwrite ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageName().c_str(),
        editing_file.c_str()
    );

    const string prev_text = editor.GetText();
    const bool is_parent_focused = IsWindowFocused();
    PushFont(gFonts.FixedWidth);
    PushStyleColor(ImGuiCol_ChildBg, editor.GetColor(PaletteIndex::Background));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    BeginChild("TextBuffer", {}, false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);
    editor.Render(is_parent_focused);
    EndChild();
    PopStyleVar();
    PopStyleColor();
    PopFont();

    // TODO this is not the usual immediate-mode case. Only set text if the text changed.
    //   This strategy of computing two full copies of the text is only temporary.
    //   Soon I'm incorporating the TextBuffer state/undo/redo system into the FlowGrid system.
    if (const string new_text = editor.GetText(); new_text != prev_text) {
        Text.IssueSet(new_text);
    } else if (Text != new_text) {
        editor.SetText(Text);
        editor.SetFilePath(string(LastOpenedFilePath));
    }
}

void TextEditor::RenderDebug() const {
    Buffer->DebugPanel();
}
