#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "Core/Store/StoreAction.h"
#include "UI/Widgets.h"

using namespace ImGui;

void AudioGraph::Render() const {
    if (BeginTabBar("")) {
        if (BeginTabItem(Nodes.ImGuiLabel.c_str())) {
            Nodes.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Connections")) {
            RenderConnections();
            EndTabItem();
        }
        EndTabBar();
    }
}

void AudioGraph::Nodes::Render() const {
    for (const auto *node : *this) {
        if (TreeNodeEx(node->ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            node->Draw();
            TreePop();
        }
    }
}

void AudioGraph::RenderConnections() const {
    const auto &style = Style.Matrix;
    const float cell_size = style.CellSize * GetTextLineHeight();
    const float cell_gap = style.CellGap;
    const float label_size = style.LabelSize * GetTextLineHeight(); // Does not include padding.
    const float label_padding = ImGui::GetStyle().ItemInnerSpacing.x;
    const float max_label_w = label_size + 2 * label_padding;
    const ImVec2 grid_top_left = GetCursorScreenPos() + max_label_w;

    BeginGroup();
    // Draw the source channel labels.
    Count source_count = 0;
    for (const auto *source_node : Nodes) {
        if (!source_node->IsSource()) continue;

        const char *label = source_node->Name.c_str();
        const string ellipsified_label = Ellipsify(string(label), label_size);

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * source_count, -max_label_w});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, max_label_w}, source_node->ImGuiLabel.c_str());
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            grid_top_left + ImVec2{(cell_size + cell_gap) * source_count + (cell_size - GetTextLineHeight()) / 2, -label_padding},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label);
        source_count++;
    }

    // Draw the destination channel labels and mixer cells.
    Count dest_i = 0;
    for (const auto *dest_node : Nodes) {
        if (!dest_node->IsDestination()) continue;

        const char *label = dest_node->Name.c_str();
        const string ellipsified_label = Ellipsify(string(label), label_size);

        SetCursorScreenPos(grid_top_left + ImVec2{-max_label_w, (cell_size + cell_gap) * dest_i});
        const auto label_interaction_flags = fg::InvisibleButton({max_label_w, cell_size}, dest_node->ImGuiLabel.c_str());
        const float label_w = CalcTextSize(ellipsified_label.c_str()).x;
        SetCursorPos(GetCursorPos() + ImVec2{max_label_w - label_w - label_padding, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.
        TextUnformatted(ellipsified_label.c_str());
        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label);

        for (Count source_i = 0; source_i < source_count; source_i++) {
            PushID(dest_i * source_count + source_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * source_i, (cell_size + cell_gap) * dest_i});
            const auto flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (flags & InteractionFlags_Clicked) Action::SetValue{Connections.PathAt(dest_i, source_i), !Connections(dest_i, source_i)}.q();

            const auto fill_color = flags & InteractionFlags_Held ? ImGuiCol_ButtonActive : (flags & InteractionFlags_Hovered ? ImGuiCol_ButtonHovered : (Connections(dest_i, source_i) ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg));
            RenderFrame(GetItemRectMin(), GetItemRectMax(), GetColorU32(fill_color));
            PopID();
        }
        dest_i++;
    }
    EndGroup();
}

void AudioGraph::Style::Matrix::Render() const {
    CellSize.Draw();
    CellGap.Draw();
    LabelSize.Draw();
}
