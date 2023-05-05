#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Cursor info")) {
        DragInt("Cursor count", &EditorState.mCurrentCursor);
        for (int i = 0; i <= EditorState.mCurrentCursor; i++) {
            DragInt2("Cursor", &EditorState.mCursors[i].mCursorPosition.mLine);
            DragInt2("Selection start", &EditorState.mCursors[i].mSelectionStart.mLine);
            DragInt2("Selection end", &EditorState.mCursors[i].mSelectionEnd.mLine);
            DragInt2("Interactive start", &EditorState.mCursors[i].mInteractiveStart.mLine);
            DragInt2("Interactive end", &EditorState.mCursors[i].mInteractiveEnd.mLine);
        }
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", UndoBuffer.size());
        DragInt("Undo index", &EditorState.mCurrentCursor);
        for (size_t i = 0; i < UndoBuffer.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (size_t j = 0; j < UndoBuffer[i].mOperations.size(); j++) {
                    TextUnformatted(UndoBuffer[i].mOperations[j].mText.c_str());
                    TextUnformatted(UndoBuffer[i].mOperations[j].mType == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    DragInt2("Start", &UndoBuffer[i].mOperations[j].mStart.mLine);
                    DragInt2("End", &UndoBuffer[i].mOperations[j].mEnd.mLine);
                    Separator();
                }
            }
        }
    }
}
