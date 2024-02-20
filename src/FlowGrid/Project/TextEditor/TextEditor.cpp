#include "TextEditor.h"

#include "imgui.h"

#include "Helper/File.h"
#include "Project/FileDialog/FileDialog.h"

TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, FileConfig &&file_config, const fs::path &file_path)
    : ActionableComponent(std::move(args)), _LastOpenedFilePath(file_path), FileDialog(file_dialog), FileConf(std::move(file_config)) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
}

TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, const fs::path &file_path)
    : TextEditor(std::move(args), file_dialog, CreateDefaultFileConfig(file_path), file_path) {}

TextEditor::~TextEditor() {}

TextEditor::FileConfig TextEditor::CreateDefaultFileConfig(const fs::path &path) const {
    return {
        {
            .owner_path = path,
            .title = "Open file",
            .filters = Buffer.GetLanguageFileExtensionsFilter(),
        },
        {
            .owner_path = path,
            .title = "Save file",
            .filters = Buffer.GetLanguageFileExtensionsFilter(),
            .default_file_name = "my_json",
            .save_mode = true,
        },
    };
}

void TextEditor::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::TextEditor::Set &a) { Buffer.SetText(a.value); },
            [this](const Action::TextEditor::ShowOpenDialog &) { FileDialog.Set(FileConf.OpenConfig); },
            [this](const Action::TextEditor::ShowSaveDialog &) { FileDialog.Set(FileConf.SaveConfig); },
            [this](const Action::TextEditor::Open &a) { Buffer.OpenFile(a.file_path); },
            [this](const Action::TextEditor::Save &a) { FileIO::write(a.file_path, Buffer.GetText()); },
        },
        action
    );
}

bool TextEditor::Empty() const { return Buffer.Empty(); }
string TextEditor::GetText() const { return Buffer.GetText(); }

using namespace ImGui;

void TextEditor::RenderMenu() const {
    if (BeginMenuBar()) {
        FileMenu.Draw();
        Buffer.RenderMenu();
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
    Buffer.Render();
}

void TextEditor::RenderDebug() const { Buffer.RenderDebug(); }
