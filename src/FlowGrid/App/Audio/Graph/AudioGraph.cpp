#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include "miniaudio.h"

#include "App/Audio/AudioDevice.h"
#include "Core/Store/StoreAction.h"
#include "UI/Widgets.h"

static ma_node_graph_config NodeGraphConfig;

using std::vector;

static ma_node_graph NodeGraph;
static ma_audio_buffer_ref InputBuffer;

void AudioGraph::AudioCallback(ma_device *device, void *output, const void *input, Count frame_count) {
    ma_audio_buffer_ref_set_data(&InputBuffer, input, frame_count);
    ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

void AudioGraph::Init() const {
    NodeGraphConfig = ma_node_graph_config_init(audio_device.InChannels);
    const int result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));

    Nodes.Init();
    vector<Primitive> connections{};
    Count dest_count = 0;
    for (const auto *dest_node : Nodes) {
        if (!dest_node->IsDestination()) continue;
        for (const auto *source_node : Nodes) {
            if (!source_node->IsSource()) continue;
            const bool default_connected =
                (source_node == &Nodes.Input && dest_node == &Nodes.Faust) ||
                (source_node == &Nodes.Faust && dest_node == &Nodes.Output);
            connections.push_back(default_connected);
        }
        dest_count++;
    }
    Action::SetMatrix{Connections.Path, connections, dest_count}.q(true);
}

void AudioGraph::Update() const {
    Nodes.Update();

    // Setting up busses is idempotent.
    Count source_i = 0;
    for (const auto *source_node : Nodes) {
        if (!source_node->IsSource()) continue;
        ma_node_detach_output_bus(source_node->Get(), 0); // No way to just detach one connection.
        Count dest_i = 0;
        for (const auto *dest_node : Nodes) {
            if (!dest_node->IsDestination()) continue;
            if (Connections(dest_i, source_i)) {
                ma_node_attach_output_bus(source_node->Get(), 0, dest_node->Get(), 0);
            }
            dest_i++;
        }
        source_i++;
    }
}
void AudioGraph::Uninit() const {
    Nodes.Uninit();
    // ma_node_graph_uninit(&NodeGraph, nullptr); // Graph endpoint is already uninitialized in `Nodes.Uninit`.
}

void AudioGraph::Nodes::Init() const {
    Output.Set(ma_node_graph_get_endpoint(&NodeGraph)); // Output is present whenever the graph is running. todo Graph is a Node
    for (const auto *node : *this) node->Init(&NodeGraph);
}
void AudioGraph::Nodes::Update() const {
    for (const auto *node : *this) node->Update(&NodeGraph);
}
void AudioGraph::Nodes::Uninit() const {
    for (const auto *node : *this) node->Uninit();
}

// Output node is already allocated by the MA graph, so we don't need to track internal data for it.
void AudioGraph::InputNode::DoInit(ma_node_graph *graph) const {
    int result = ma_audio_buffer_ref_init((ma_format) int(audio_device.InFormat), audio_device.InChannels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize input audio buffer: ", result));

    static ma_data_source_node Node{};
    static ma_data_source_node_config Config{};

    Config = ma_data_source_node_config_init(&InputBuffer);
    result = ma_data_source_node_init(graph, &Config, nullptr, &Node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    Set(&Node);
}
void AudioGraph::InputNode::DoUninit() const {
    ma_data_source_node_uninit((ma_data_source_node *)Get(), nullptr);
    ma_audio_buffer_ref_uninit(&InputBuffer);
}

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
