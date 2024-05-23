#include "TextEditor.h"

#include "imgui.h"

#include "Helper/File.h"
#include "Project/FileDialog/FileDialog.h"

TextEditor::TextEditor(ArgsT &&args, const ::FileDialog &file_dialog, const fs::path &file_path)
    : ActionableComponent(std::move(args)), FileDialog(file_dialog), _LastOpenedFilePath(file_path) {
    WindowFlags |= ImGuiWindowFlags_MenuBar;
}

TextEditor::~TextEditor() {}

// todo when multiple buffers are supported, apply actions to focused buffer.
void TextEditor::Apply(const ActionType &action) const { Buffer.Apply(action); }
bool TextEditor::CanApply(const ActionType &action) const { return Buffer.CanApply(action); }

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
