#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Cursor info")) {
        DragInt("Cursor count", &mState.mCurrentCursor);
        for (int i = 0; i <= mState.mCurrentCursor; i++) {
            DragInt2("Cursor", &mState.mCursors[i].mCursorPosition.mLine);
            DragInt2("Selection start", &mState.mCursors[i].mSelectionStart.mLine);
            DragInt2("Selection end", &mState.mCursors[i].mSelectionEnd.mLine);
            DragInt2("Interactive start", &mState.mCursors[i].mInteractiveStart.mLine);
            DragInt2("Interactive end", &mState.mCursors[i].mInteractiveEnd.mLine);
        }
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", mUndoBuffer.size());
        DragInt("Undo index", &mState.mCurrentCursor);
        for (size_t i = 0; i < mUndoBuffer.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (size_t j = 0; j < mUndoBuffer[i].mOperations.size(); j++) {
                    TextUnformatted(mUndoBuffer[i].mOperations[j].mText.c_str());
                    TextUnformatted(mUndoBuffer[i].mOperations[j].mType == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    DragInt2("Start", &mUndoBuffer[i].mOperations[j].mStart.mLine);
                    DragInt2("End", &mUndoBuffer[i].mOperations[j].mEnd.mLine);
                    Separator();
                }
            }
        }
    }
}
