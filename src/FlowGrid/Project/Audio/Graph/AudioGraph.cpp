#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "Core/Container/AdjacencyListAction.h"
#include "Project/Audio/AudioDevice.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

using std::vector;

static ma_node_graph NodeGraph;
static ma_node_graph_config NodeGraphConfig;
static ma_audio_buffer_ref InputBuffer;

AudioGraph::AudioGraph(ComponentArgs &&args) : Component(std::move(args)) {
    Init();
    const Field::References listened_fields = {audio_device.InChannels, audio_device.OutChannels, audio_device.InFormat, audio_device.OutFormat, Connections};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    for (const auto *node : Nodes) node->On.RegisterChangeListener(this);
}
AudioGraph::~AudioGraph() {
    Uninit();
}

void AudioGraph::OnFieldChanged() {
    if (Connections.IsChanged()) {
        UpdateConnections();
        return;
    }
    for (auto *node : Nodes) {
        if (node->On.IsChanged()) {
            node->Update();
            UpdateConnections();
            return;
        }
    }

    // Device field changed.
    Uninit();
    Init();
    Update();
}

void AudioGraph::AudioCallback(ma_device *device, void *output, const void *input, Count frame_count) {
    ma_audio_buffer_ref_set_data(&InputBuffer, input, frame_count);
    ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

ma_node_graph *AudioGraph::Get() const { return &NodeGraph; }

void AudioGraph::Init() {
    NodeGraphConfig = ma_node_graph_config_init(audio_device.InChannels);
    const int result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);

    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));

    Nodes.Init();
    Connections.Connect(Nodes.Input.Id, Nodes.Faust.Id);
    Connections.Connect(Nodes.Faust.Id, Nodes.Output.Id);
}

void AudioGraph::UpdateConnections() {
    for (auto *source_node : Nodes) {
        if (!source_node->IsSource()) continue;

        source_node->DisconnectOutputs();

        ma_node *prev_dest_node = nullptr;
        for (auto *dest_node : Nodes) {
            if (!dest_node->IsDestination()) continue;

            if (Connections.IsConnected(source_node->Id, dest_node->Id)) {
                if (prev_dest_node) {
                    // Connecting a single source to multiple destinations requires a splitter node.
                    // We chain splitters together to support any number of destinations.
                    // Note: `new` is necessary here because we use a custom deleter.
                    source_node->SplitterNodes.emplace_back(new ma_splitter_node());
                    ma_splitter_node *splitter_node = (ma_splitter_node *)source_node->SplitterNodes.back().get();
                    ma_splitter_node_config splitter_node_config = ma_splitter_node_config_init(source_node->OutputChannelCount(0));
                    int result = ma_splitter_node_init(Get(), &splitter_node_config, nullptr, splitter_node);
                    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", result));

                    ma_node_attach_output_bus(splitter_node, 0, prev_dest_node, 0);
                    ma_node_attach_output_bus(splitter_node, 1, dest_node->Node, 0);
                    ma_node_attach_output_bus(source_node->Node, 0, splitter_node, 0);
                    prev_dest_node = splitter_node;
                } else {
                    source_node->ConnectTo(*dest_node);
                    prev_dest_node = dest_node->Node;
                }
            }
        }
    }
}

void AudioGraph::Update() {
    Nodes.Update();
    UpdateConnections();
}

void AudioGraph::Uninit() {
    Nodes.Uninit();
    // Graph node is already uninitialized in `Nodes.Uninit`.
}

AudioGraph::Nodes::Nodes(ComponentArgs &&args) : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {}
AudioGraph::Nodes::~Nodes() {}

void AudioGraph::Nodes::Init() {
    for (auto *node : *this) node->Init();
}
void AudioGraph::Nodes::Update() {
    for (auto *node : *this) node->Update();
}
void AudioGraph::Nodes::Uninit() {
    for (auto *node : *this) node->Uninit();
}

AudioGraph::InputNode::InputNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Muted.Set_(true); // External input is muted by default.
}

ma_node *AudioGraph::InputNode::DoInit() {
    int result = ma_audio_buffer_ref_init((ma_format) int(audio_device.InFormat), audio_device.InChannels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize input audio buffer: ", result));

    static ma_data_source_node source_node{};
    ma_data_source_node_config config = ma_data_source_node_config_init(&InputBuffer);
    result = ma_data_source_node_init(Graph->Get(), &config, nullptr, &source_node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    return &source_node;
}
void AudioGraph::InputNode::DoUninit() {
    ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
    ma_audio_buffer_ref_uninit(&InputBuffer);
}

// The output node is the graph endpoint. It's allocated and managed by the MA graph.
ma_node *AudioGraph::OutputNode::DoInit() {
    return ma_node_graph_get_endpoint(Graph->Get());
}

void AudioGraph::Nodes::Render() const {
    RenderTreeNodes();
}

using namespace ImGui;

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

        Count source_i = 0;
        for (const auto *source_node : Nodes) {
            if (!source_node->IsSource()) continue;

            PushID(dest_i * source_count + source_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * source_i, (cell_size + cell_gap) * dest_i});

            const bool disabled = source_node->Id == dest_node->Id;
            if (disabled) BeginDisabled();

            const auto flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (flags & InteractionFlags_Clicked) {
                Action::AdjacencyList::ToggleConnection{Connections.Path, source_node->Id, dest_node->Id}.q();
            }

            const bool is_connected = Connections.IsConnected(source_node->Id, dest_node->Id);
            const auto fill_color =
                flags & InteractionFlags_Held ?
                ImGuiCol_ButtonActive :
                (flags & InteractionFlags_Hovered ?
                     ImGuiCol_ButtonHovered :
                     (is_connected ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg)
                );
            RenderFrame(GetItemRectMin(), GetItemRectMax(), GetColorU32(fill_color));

            if (disabled) EndDisabled();

            PopID();
            source_i++;
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
