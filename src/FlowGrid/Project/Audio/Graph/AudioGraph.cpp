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
    const Field::References listened_fields = {audio_device.On, audio_device.InChannels, audio_device.OutChannels, audio_device.InFormat, audio_device.OutFormat, Connections};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    for (const auto *node : Nodes) {
        // Changing these node fields can result in connection changes.
        node->On.RegisterChangeListener(this);
        node->SmoothOutputLevel.RegisterChangeListener(this);
        node->Monitor.RegisterChangeListener(this);
    }
}
AudioGraph::~AudioGraph() {
    Uninit();
}

void AudioGraph::OnFieldChanged() {
    if (audio_device.IsChanged()) {
        Uninit();
        Init();
        Update();
        return; // Nodes and connections are already updated.
    }

    bool any_node_changed = false;
    for (auto *node : Nodes) {
        if (node->On.IsChanged() || node->Monitor.IsChanged() || node->SmoothOutputLevel.IsChanged()) {
            node->Update();
            any_node_changed = true;
        }
    }

    if (Connections.IsChanged() || any_node_changed) {
        UpdateConnections();
    }
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
    for (auto *out_node : Nodes) {
        out_node->DisconnectAll();
    }

    for (auto *out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        for (auto *in_node : Nodes) {
            if (in_node->InputBusCount() == 0) continue;

            if (Connections.IsConnected(out_node->Id, in_node->Id)) {
                out_node->ConnectTo(*in_node);
            }
        }
    }

    // Update node active states.
    // Nodes that are turned off (here - disabled) are not removed from the `Connections` object in order to preserve their connections.
    // So we need to check if there is a path to the output node that doesn't go through any disabled nodes.
    std::unordered_set<ID> disabled_node_ids;
    for (const auto *node : Nodes) {
        if (!node->On) disabled_node_ids.insert(node->Id);
    }
    for (auto *node : Nodes) {
        node->SetActive(audio_device.On && Connections.HasPath(node->Id, Nodes.Output.Id, disabled_node_ids));
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

AudioGraph::Nodes::Nodes(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {}

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
    // Calculate the maximum I/O label widths.
    ImVec2 max_label_w_no_padding{0, 0}; // in (left), out (top)
    for (const auto *node : Nodes) {
        const float label_w = CalcTextSize(node->Name.c_str()).x;
        if (node->InputBusCount() > 0) max_label_w_no_padding.x = std::max(max_label_w_no_padding.x, label_w);
        if (node->OutputBusCount() > 0) max_label_w_no_padding.y = std::max(max_label_w_no_padding.y, label_w);
    }

    const auto &style = Style.Matrix;
    const float max_allowed_label_w = style.MaxLabelSpace * GetTextLineHeight();
    const ImVec2 label_w_no_padding = {std::min(max_allowed_label_w, max_label_w_no_padding.x), std::min(max_allowed_label_w, max_label_w_no_padding.y)};
    const float label_padding = ImGui::GetStyle().ItemInnerSpacing.x;
    const ImVec2 label_w = label_w_no_padding + 2 * label_padding;
    const ImVec2 grid_top_left = GetCursorScreenPos() + label_w;
    const float cell_size = style.CellSize * GetTextLineHeight();
    const float cell_gap = style.CellGap;

    BeginGroup();

    // Output channel labels.
    Count out_count = 0;
    for (const auto *out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_count, -label_w.y});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, label_w.y}, out_node->ImGuiLabel.c_str());
    
        const string label = out_node->Name;
        const string ellipsified_label = Ellipsify(label, label_w_no_padding.y);
        const bool is_active = out_node->IsActive;
        if (!is_active) BeginDisabled();
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            grid_top_left + ImVec2{(cell_size + cell_gap) * out_count + (cell_size - GetTextLineHeight()) / 2, -label_padding},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label.c_str());
        out_count++;
    }

    // Input channel labels and mixer cells.
    Count in_i = 0;
    for (const auto *in_node : Nodes) {
        if (in_node->InputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{-label_w.x, (cell_size + cell_gap) * in_i});
        const auto label_interaction_flags = fg::InvisibleButton({label_w.x, cell_size}, in_node->ImGuiLabel.c_str());
    
        const char *label = in_node->Name.c_str();
        const string ellipsified_label = Ellipsify(label, label_w_no_padding.x);
        SetCursorPos(GetCursorPos() + ImVec2{label_w.x - CalcTextSize(ellipsified_label.c_str()).x - label_padding, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.

        const bool is_active = in_node->IsActive;
        if (!is_active) BeginDisabled();
        TextUnformatted(ellipsified_label.c_str());
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label);

        Count out_i = 0;
        for (const auto *out_node : Nodes) {
            if (out_node->OutputBusCount() == 0) continue;

            PushID(in_i * out_count + out_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_i, (cell_size + cell_gap) * in_i});

            const bool disabled = out_node->Id == in_node->Id;
            if (disabled) BeginDisabled();

            const auto flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (flags & InteractionFlags_Clicked) {
                Action::AdjacencyList::ToggleConnection{Connections.Path, out_node->Id, in_node->Id}.q();
            }

            const bool is_connected = Connections.IsConnected(out_node->Id, in_node->Id);
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
            out_i++;
        }
        in_i++;
    }
    EndGroup();
}

void AudioGraph::Style::Matrix::Render() const {
    CellSize.Draw();
    CellGap.Draw();
    MaxLabelSpace.Draw();
}
