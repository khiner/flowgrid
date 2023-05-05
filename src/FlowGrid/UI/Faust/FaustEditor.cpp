#include "TextEditor.h"

#include "../../App.h"

static const Menu FileMenu = {"File", {ShowOpenFaustFileDialog{}, ShowSaveFaustFileDialog{}}};

using namespace ImGui;

static TextEditor editor;

void Faust::FaustEditor::Render() const {
    if (ImGui::BeginMenuBar()) {
        FileMenu.Draw();
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Read-only mode", nullptr, &editor.ReadOnly);
            ImGui::Separator();
            if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !editor.ReadOnly && editor.CanUndo())) editor.Undo();
            if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !editor.ReadOnly && editor.CanRedo())) editor.Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection())) editor.Copy();
            if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !editor.ReadOnly && editor.HasSelection())) editor.Cut();
            if (ImGui::MenuItem("Delete", "Del", nullptr, !editor.ReadOnly && editor.HasSelection())) editor.Delete();
            if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !editor.ReadOnly && ImGui::GetClipboardText() != nullptr)) editor.Paste();
            ImGui::Separator();
            if (ImGui::MenuItem("Select all", nullptr, nullptr)) {
                editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Mariana palette")) editor.SetPalette(TextEditor::GetMarianaPalette());
            if (ImGui::MenuItem("Dark palette")) editor.SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light palette")) editor.SetPalette(TextEditor::GetLightPalette());
            if (ImGui::MenuItem("Retro blue palette")) editor.SetPalette(TextEditor::GetRetroBluePalette());
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    static bool initialized = false;
    static auto lang = TextEditor::LanguageDefinition::CPlusPlus();

    if (!initialized) {
        initialized = true;
        editor.SetLanguageDefinition(lang);
    }
    auto cpos = editor.GetCursorPosition();

    const string editing_file = "no file";
    ImGui::Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
        editor.Overwrite ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageDefinitionName(),
        editing_file.c_str()
    );

    ImGui::PushFont(UiContext.Fonts.FixedWidth);
    editor.Render("TextEditor");
    ImGui::PopFont();

    const auto text = editor.GetText();
    if (editor.TextChanged) {
        q(SetValue{s.Faust.Code.Path, text});
    } else if (s.Faust.Code != text) {
        // TODO this is not the usual immediate-mode case. Only set text if the text changed.
        //   Really what I want is to incorporate the ImGuiColorTextEdit undo/redo system into the FlowGrid system.
        editor.SetText(s.Faust.Code);
    }
}

using namespace ImGui;

void Faust::FaustEditor::Metrics::Render() const {
    if (CollapsingHeader("Cursor info")) {
        DragInt("Cursor count", &editor.mState.mCurrentCursor);
        for (int i = 0; i <= editor.mState.mCurrentCursor; i++) {
            DragInt2("Cursor", &editor.mState.mCursors[i].mCursorPosition.mLine);
            DragInt2("Selection start", &editor.mState.mCursors[i].mSelectionStart.mLine);
            DragInt2("Selection end", &editor.mState.mCursors[i].mSelectionEnd.mLine);
            DragInt2("Interactive start", &editor.mState.mCursors[i].mInteractiveStart.mLine);
            DragInt2("Interactive end", &editor.mState.mCursors[i].mInteractiveEnd.mLine);
        }
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", editor.mUndoBuffer.size());
        DragInt("Undo index", &editor.mState.mCurrentCursor);
        for (size_t i = 0; i < editor.mUndoBuffer.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (size_t j = 0; j < editor.mUndoBuffer[i].mOperations.size(); j++) {
                    TextUnformatted(editor.mUndoBuffer[i].mOperations[j].mText.c_str());
                    TextUnformatted(editor.mUndoBuffer[i].mOperations[j].mType == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    DragInt2("Start", &editor.mUndoBuffer[i].mOperations[j].mStart.mLine);
                    DragInt2("End", &editor.mUndoBuffer[i].mOperations[j].mEnd.mLine);
                    Separator();
                }
            }
        }
    }
}
