#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Cursor info")) {
        DragInt("Cursor count", &EditorState.CurrentCursor);
        for (int i = 0; i <= EditorState.CurrentCursor; i++) {
            DragInt2("Cursor", &EditorState.Cursors[i].CursorPosition.mLine);
            DragInt2("Selection start", &EditorState.Cursors[i].SelectionStart.mLine);
            DragInt2("Selection end", &EditorState.Cursors[i].SelectionEnd.mLine);
            DragInt2("Interactive start", &EditorState.Cursors[i].InteractiveStart.mLine);
            DragInt2("Interactive end", &EditorState.Cursors[i].InteractiveEnd.mLine);
        }
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", UndoBuffer.size());
        DragInt("Undo index", &EditorState.CurrentCursor);
        for (size_t i = 0; i < UndoBuffer.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (size_t j = 0; j < UndoBuffer[i].Operations.size(); j++) {
                    TextUnformatted(UndoBuffer[i].Operations[j].Text.c_str());
                    TextUnformatted(UndoBuffer[i].Operations[j].mType == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    DragInt2("Start", &UndoBuffer[i].Operations[j].Start.mLine);
                    DragInt2("End", &UndoBuffer[i].Operations[j].End.mLine);
                    Separator();
                }
            }
        }
    }
}
