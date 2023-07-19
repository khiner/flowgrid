#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "miniaudio.h"

#include "Core/Container/AdjacencyListAction.h"
#include "Project/Audio/AudioInputDevice.h"
#include "Project/Audio/AudioOutputDevice.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/TestToneNode.h"

static const AudioGraph *Singleton;

// Wraps around `ma_node_graph`.
struct MaGraph {
    MaGraph(u32 in_channels) {
        auto config = ma_node_graph_config_init(in_channels);
        _Graph = std::make_unique<ma_node_graph>();
        const int result = ma_node_graph_init(&config, nullptr, _Graph.get());

        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));
    }
    ~MaGraph() {
        // Graph endpoint node is already uninitialized by `Nodes.Output`.
        _Graph.reset();
    }

    inline ma_node_graph *Get() const noexcept { return _Graph.get(); }

private:
    std::unique_ptr<ma_node_graph> _Graph;
};

struct Buffer {
    Buffer(ma_format format, u32 channels) {
        int result = ma_audio_buffer_ref_init(format, channels, nullptr, 0, &BufferRef);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize buffer ref: ", result));
    }
    ~Buffer() {
        ma_audio_buffer_ref_uninit(&BufferRef);
    }

    void SetData(const void *input, u32 frame_count) {
        ma_audio_buffer_ref_set_data(&BufferRef, input, frame_count);
    }

    ma_audio_buffer_ref *Get() noexcept { return &BufferRef; }

private:
    ma_audio_buffer_ref BufferRef;
};

// A `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref`.
struct SourceBufferNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    SourceBufferNode(ComponentArgs &&args, ma_format format, u32 channels) : AudioGraphNode(std::move(args)) {
        InitBuffer(std::move(format), std::move(channels));
    }

    ma_node *DoInit(ma_node_graph *graph) override {
        auto config = ma_data_source_node_config_init(_Buffer->Get());
        int result = ma_data_source_node_init(graph, &config, nullptr, &source_node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

        return &source_node;
    }

    void DoUninit() override {
        _Buffer.reset();
        ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
    }

    void InitBuffer(ma_format format, u32 channels) {
        _Buffer = std::make_unique<Buffer>(std::move(format), std::move(channels));
    }

    void SetBufferData(const void *input, u32 frame_count) const {
        if (_Buffer) _Buffer->SetData(input, frame_count);
    }

private:
    std::unique_ptr<Buffer> _Buffer;
    ma_data_source_node source_node;
};

// A source node that owns an input device and copies its callback buffer to its own buffer.
struct DeviceInputNode : SourceBufferNode {
    DeviceInputNode(ComponentArgs &&args) : SourceBufferNode(std::move(args)) {
        Muted.Set_(true); // External input is muted by default.
        InputDevice = std::make_unique<AudioInputDevice>(ComponentArgs{this, "InputDevice"}, AudioInputCallback, this);
        InitBuffer(ma_format(int(InputDevice->Format)), InputDevice->Channels);
    }

    static void AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *self = reinterpret_cast<DeviceInputNode *>(device->pUserData);
        self->SetBufferData(input, frame_count);
        (void)output;
    }

    std::unique_ptr<AudioInputDevice> InputDevice;
};

// A node that writes directly to an output device's buffer.
// Must always be connected directly to the graph output node.
// struct DeviceOutputNode : PassThroughBufferNode {
//     using PassThroughBufferNode::PassThroughBufferNode;
// };

void AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
    if (Singleton) ma_node_graph_read_pcm_frames(Singleton->Get(), output, frame_count, nullptr);
    (void)device;
    (void)input;
}

// The output node is the graph endpoint. It's allocated and managed by the MA graph.
struct OutputNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    ma_node *DoInit(ma_node_graph *graph) override {
        return ma_node_graph_get_endpoint(graph);
    }
};

AudioGraph::AudioGraph(ComponentArgs &&args) : Component(std::move(args)) {
    Nodes.push_back(std::make_unique<DeviceInputNode>(ComponentArgs{this, "Input"}));

    const AudioInputDevice *input_device = GetDeviceInputNode()->InputDevice.get();
    OutputDevice = std::make_unique<AudioOutputDevice>(ComponentArgs{this, "OutputDevice"}, AudioOutputCallback);
    Graph = std::make_unique<MaGraph>(input_device->Channels);
    Nodes.push_back(std::make_unique<OutputNode>(ComponentArgs{this, "Output"}));

    Nodes.push_back(std::make_unique<FaustNode>(ComponentArgs{this, "Faust"}));
    Nodes.push_back(std::make_unique<TestToneNode>(ComponentArgs{this, "TestTone"}));
    for (const auto &node : Nodes) node->Init();

    const Field::References listened_fields = {
        input_device->On,
        input_device->Channels,
        input_device->SampleRate,
        input_device->Format,
        OutputDevice->On,
        OutputDevice->Channels,
        OutputDevice->SampleRate,
        OutputDevice->Format,
        Connections,
    };
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    for (const auto &node : Nodes) node->RegisterListener(this);

    // Set up default connections.
    // Connections.Connect(Nodes.Input.Id, Nodes.Faust.Id);
    // Connections.Connect(Nodes.Faust.Id, Nodes.Output.Id);
    Connections.Connect(GetDeviceInputNode()->Id, GetGraphOutputNode()->Id);
    Singleton = this;
}

AudioGraph::~AudioGraph() {
    Singleton = nullptr;
    for (const auto &node : Nodes) {
        node->UnregisterListener(this);
        node->Uninit();
    }
    Field::UnregisterChangeListener(this);
}
ma_node_graph *AudioGraph::Get() const { return Graph->Get(); }

// xxx depending on dynamic node positions is temporary.
DeviceInputNode *AudioGraph::GetDeviceInputNode() const { return static_cast<DeviceInputNode *>(Nodes[0].get()); }
OutputNode *AudioGraph::GetGraphOutputNode() const { return static_cast<OutputNode *>(Nodes[1].get()); }

void AudioGraph::OnFaustDspChanged(dsp *dsp) {
    // xxx depending on dynamic node positions is temporary.
    static_cast<FaustNode *>(Nodes[2].get())->OnFaustDspChanged(dsp);
    UpdateConnections();
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) {
    UpdateConnections();
}

void AudioGraph::OnFieldChanged() {
    const AudioInputDevice *input_device = GetDeviceInputNode()->InputDevice.get();
    if (input_device->On.IsChanged() ||
        input_device->Channels.IsChanged() ||
        input_device->Format.IsChanged() ||
        input_device->SampleRate.IsChanged() ||
        OutputDevice->On.IsChanged() ||
        OutputDevice->Channels.IsChanged() ||
        OutputDevice->Format.IsChanged() ||
        OutputDevice->SampleRate.IsChanged()) {
        for (const auto &node : Nodes) node->Uninit();
        Graph = std::make_unique<MaGraph>(input_device->Channels);
        for (const auto &node : Nodes) node->Init();
        UpdateConnections();
        return;
    }
    // if (input_device->SampleRate.IsChanged() || OutputDevice->SampleRate.IsChanged()) {
    //     for (const auto &node : Nodes) node->OnDeviceSampleRateChanged();
    // }

    if (Connections.IsChanged()) {
        UpdateConnections();
    }
}

u32 AudioGraph::GetDeviceSampleRate() const { return OutputDevice->SampleRate; }
u32 AudioGraph::GetDeviceBufferSize() const { return OutputDevice->Get()->playback.internalPeriodSizeInFrames; }

void AudioGraph::UpdateConnections() {
    for (const auto &out_node : Nodes) out_node->DisconnectAll();

    for (const auto &out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        for (const auto &in_node : Nodes) {
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
    for (const auto &node : Nodes) {
        if (!node->On) disabled_node_ids.insert(node->Id);
    }
    for (const auto &node : Nodes) {
        node->SetActive(OutputDevice->On && Connections.HasPath(node->Id, GetGraphOutputNode()->Id, disabled_node_ids));
    }
}

using namespace ImGui;

void AudioGraph::RenderConnections() const {
    // Calculate the maximum I/O label widths.
    ImVec2 max_label_w_no_padding{0, 0}; // I/O vec: in (left), out (top)
    for (const auto &node : Nodes) {
        const float label_w = CalcTextSize(node->Name.c_str()).x;
        if (node->InputBusCount() > 0) max_label_w_no_padding.x = std::max(max_label_w_no_padding.x, label_w);
        if (node->OutputBusCount() > 0) max_label_w_no_padding.y = std::max(max_label_w_no_padding.y, label_w);
    }

    const ImVec2 label_padding = ImVec2{ImGui::GetStyle().ItemInnerSpacing.x, 0} + ImGui::GetStyle().FramePadding;

    const auto &style = Style.Matrix;
    const float max_allowed_label_w = style.MaxLabelSpace * GetTextLineHeight();
    const ImVec2 node_label_w_no_padding = {std::min(max_allowed_label_w, max_label_w_no_padding.x), std::min(max_allowed_label_w, max_label_w_no_padding.y)};
    const ImVec2 node_label_w = node_label_w_no_padding + label_padding.x * 2; // I/O vec
    const float fhws = GetFrameHeightWithSpacing();
    const auto og_cursor_pos = GetCursorScreenPos();
    const ImVec2 grid_top_left = og_cursor_pos + node_label_w + fhws; // Last line-height is for the I/O header labels.

    BeginGroup();

    static const string InputsLabel = "Inputs";
    static const string OutputsLabel = "Outputs";
    // I/O header frames + labels on the left/top, respectively.
    const ImVec2 io_header_w_no_padding = ImVec2{CalcTextSize(InputsLabel).x, CalcTextSize(OutputsLabel).x}; // I/O vec
    const ImVec2 io_header_w = io_header_w_no_padding + label_padding.x * 2; // I/O vec
    ImVec2 io_frame_w = GetContentRegionAvail() - (node_label_w + fhws); // I/O vec
    io_frame_w = ImVec2{std::max(io_frame_w.x, io_header_w.x), std::max(io_frame_w.y, io_header_w.y)};

    SetCursorScreenPos({grid_top_left.x, og_cursor_pos.y});
    RenderFrame(
        GetCursorScreenPos(),
        GetCursorScreenPos() + ImVec2{io_frame_w.x, fhws},
        GetColorU32(ImGuiCol_FrameBg)
    );
    RenderText(GetCursorScreenPos() + ImVec2{(io_frame_w.x - io_header_w.x) / 2, 0} + label_padding, InputsLabel.c_str());

    SetCursorScreenPos({og_cursor_pos.x, grid_top_left.y});
    RenderFrame(
        GetCursorScreenPos(),
        GetCursorScreenPos() + ImVec2{fhws, io_frame_w.y},
        GetColorU32(ImGuiCol_FrameBg)
    );
    ImPlot::AddTextVertical(
        GetWindowDrawList(),
        GetCursorScreenPos() + ImVec2{0, (io_frame_w.y - io_header_w.y) / 2 + io_header_w_no_padding.y} + ImVec2{label_padding.y, label_padding.x},
        GetColorU32(ImGuiCol_Text), OutputsLabel.c_str()
    );

    const float cell_size = style.CellSize * GetTextLineHeight();
    const float cell_gap = style.CellGap;

    // Output channel labels.
    u32 out_count = 0;
    for (const auto &out_node : Nodes) {
        if (out_node->OutputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_count, -node_label_w.y});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, node_label_w.y}, out_node->ImGuiLabel.c_str());

        const string label = out_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.y);
        const bool is_active = out_node->IsActive;
        if (!is_active) BeginDisabled();
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            grid_top_left + ImVec2{(cell_size + cell_gap) * out_count + (cell_size - GetTextLineHeight()) / 2, -label_padding.y},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label.c_str());
        out_count++;
    }

    // Input channel labels and mixer cells.
    u32 in_i = 0;
    for (const auto &in_node : Nodes) {
        if (in_node->InputBusCount() == 0) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{-node_label_w.x, (cell_size + cell_gap) * in_i});
        const auto label_interaction_flags = fg::InvisibleButton({node_label_w.x, cell_size}, in_node->ImGuiLabel.c_str());

        const string label = in_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.x);
        SetCursorPos(GetCursorPos() + ImVec2{node_label_w.x - CalcTextSize(ellipsified_label.c_str()).x - label_padding.y, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.

        const bool is_active = in_node->IsActive;
        if (!is_active) BeginDisabled();
        TextUnformatted(ellipsified_label.c_str());
        if (!is_active) EndDisabled();

        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label.c_str());

        u32 out_i = 0;
        for (const auto &out_node : Nodes) {
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

void AudioGraph::RenderNodes() const {
    for (const auto &node : Nodes) {
        if (TreeNodeEx(node->ImGuiLabel.c_str())) {
            node->Draw();
            TreePop();
        }
    }
}

void AudioGraph::Render() const {
    const AudioInputDevice *input_device = GetDeviceInputNode()->InputDevice.get();
    if (BeginTabItem(input_device->ImGuiLabel.c_str())) {
        input_device->Draw();
        EndTabItem();
    }
    if (BeginTabItem(OutputDevice->ImGuiLabel.c_str())) {
        OutputDevice->Draw();
        EndTabItem();
    }
    if (BeginTabItem("Nodes")) {
        RenderNodes();
        EndTabItem();
    }
    if (BeginTabItem(Connections.ImGuiLabel.c_str())) {
        RenderConnections();
        EndTabItem();
    }
}
