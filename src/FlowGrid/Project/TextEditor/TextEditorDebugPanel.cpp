#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Editor state info")) {
        Checkbox("Panning", &Panning);
        Checkbox("Dragging selection", &IsDraggingSelection);
        DragInt("Cursor count", &State.CurrentCursor);
        for (int i = 0; i <= State.CurrentCursor; i++) {
            DragInt2("Interactive start", &State.Cursors[i].InteractiveStart.Line);
            DragInt2("Interactive end", &State.Cursors[i].InteractiveEnd.Line);
        }
    }
    if (CollapsingHeader("Lines")) {
        for (int i = 0; i < Lines.size(); i++) {
            Text("%d", Lines[i].size());
        }
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", UndoBuffer.size());
        Text("Undo index: %d", UndoIndex);
        for (size_t i = 0; i < UndoBuffer.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (size_t j = 0; j < UndoBuffer[i].Operations.size(); j++) {
                    TextUnformatted(UndoBuffer[i].Operations[j].Text.c_str());
                    TextUnformatted(UndoBuffer[i].Operations[j].Type == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    DragInt2("Start", &UndoBuffer[i].Operations[j].Start.Line);
                    DragInt2("End", &UndoBuffer[i].Operations[j].End.Line);
                    Separator();
                }
            }
        }
    }
}
