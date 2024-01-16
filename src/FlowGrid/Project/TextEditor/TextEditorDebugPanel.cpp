#include "TextEditor.h"

#include "imgui.h"

using namespace ImGui;

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Editor state info")) {
        BeginDisabled();
        Checkbox("Panning", &Panning);
        Checkbox("Dragging selection", &IsDraggingSelection);
        EndDisabled();
        Text("Cursor count: %lu", Cursors.size());
        for (auto &c : Cursors) {
            const auto &start = c.GetStart(), &end = c.GetEnd();
            Text("Start: {%d, %d}", start.L, start.C);
            Text("End: {%d, %d}", end.L, end.C);
            Spacing();
        }
    }
    if (CollapsingHeader("Lines")) {
        for (uint i = 0; i < Lines.size(); i++) Text("%lu", Lines[i].size());
    }
    if (CollapsingHeader("Undo")) {
        Text("Number of records: %lu", UndoBuffer.size());
        Text("Undo index: %d", UndoIndex);
        for (size_t i = 0; i < UndoBuffer.size(); i++) {
            const auto &record = UndoBuffer[i];
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
    if (CollapsingHeader("Tree-Sitter")) {
        Text("S-expression:\n%s", GetSyntaxTreeSExp().c_str());
    }
}
