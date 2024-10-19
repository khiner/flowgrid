#include "TextEditor.h"

#include "imgui.h"

#include "Core/FileDialog/FileDialog.h"
#include "Core/Helper/File.h"

TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, const fs::path &file_path)
    : ActionProducerComponent(std::move(args)), FileDialog(file_dialog), _LastOpenedFilePath(file_path) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
}

TextEditor::~TextEditor() {}

bool TextEditor::Empty() const { return Buffer.Empty(); }
std::string TextEditor::GetText() const { return Buffer.GetText(); }

using namespace ImGui;

void TextEditor::RenderMenu() const {
    if (BeginMenuBar()) {
        Buffer.RenderMenu();
        EndMenuBar();
    }
}

void TextEditor::Render() const {
    RenderMenu();
    Buffer.Render();
}

void TextEditor::RenderDebug() const { Buffer.RenderDebug(); }
