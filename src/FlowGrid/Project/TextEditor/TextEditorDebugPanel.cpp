#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Editor state info")) {
        BeginDisabled();
        Checkbox("Panning", &Panning);
        Checkbox("Dragging selection", &IsDraggingSelection);
        EndDisabled();
        Text("Cursor count: %u", State.Cursors.size());
        for (auto &c : State.Cursors) {
            DragInt2("Interactive start", &c.InteractiveStart.L);
            DragInt2("Interactive end", &c.InteractiveEnd.L);
        }
    }
    if (CollapsingHeader("Lines")) {
        for (uint i = 0; i < Lines.size(); i++) Text("%lu", Lines[i].size());
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", UndoBuffer.size());
        Text("Undo index: %d", UndoIndex);
        for (size_t i = 0; i < UndoBuffer.size(); i++) {
            const auto& record = UndoBuffer[i];
            if (CollapsingHeader(std::to_string(i).c_str())) {
                TextUnformatted("Operations");
                for (const auto &operation : record.Operations) {
                    TextUnformatted(operation.Text.c_str());
                    TextUnformatted(operation.Type == TextEditor::UndoOperationType::Add ? "Add" : "Delete");
                    Text("Start: %d", operation.Start.L);
                    Text("End: %d", operation.End.L);
                    Separator();
                }
            }
        }
    }
}
