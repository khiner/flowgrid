#include "TextEditor.h"

#include "../App.h"
#include "../Helper/File.h"

void Editor::Render() const {
    static bool Initialized = false;
    static const char *file_to_edit = "../src/FlowGrid/UI/Editor.cpp";
    static auto lang = TextEditor::LanguageDefinition::CPlusPlus();
    static TextEditor editor;

    if (!Initialized) {
        Initialized = true;
        editor.SetLanguageDefinition(lang);
        editor.SetText(FileIO::read(file_to_edit));
    }
    auto cpos = editor.GetCursorPosition();

    ImGui::Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
        editor.IsOverwrite() ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageDefinitionName(),
        file_to_edit
    );
    editor.Render("TextEditor");
    // auto text = editor.GetText();
}
