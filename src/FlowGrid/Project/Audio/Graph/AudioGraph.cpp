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
#include "Project/Audio/WaveformNode.h"

#include "ma_data_passthrough_node/ma_data_passthrough_node.h"

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

struct BufferRef {
    BufferRef(ma_format format, u32 channels) {
        int result = ma_audio_buffer_ref_init(format, channels, nullptr, 0, &Ref);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize buffer ref: ", result));
    }
    ~BufferRef() {
        ma_audio_buffer_ref_uninit(&Ref);
    }

    u64 GetSize() const { return Ref.sizeInFrames; }

    void SetData(const void *input, u32 frame_count) {
        ma_audio_buffer_ref_set_data(&Ref, input, frame_count);
    }

    void ReadData(void *output, u32 frame_count) {
        ma_audio_buffer_ref_read_pcm_frames(&Ref, output, frame_count, false);
    }

    ma_audio_buffer_ref *Get() noexcept { return &Ref; }

private:
    ma_audio_buffer_ref Ref;
};

struct BufferRefNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    inline u64 GetBufferSize() const { return _BufferRef ? _BufferRef->GetSize() : 0; }
    inline void InitBuffer(u32 channels) { _BufferRef = std::make_unique<BufferRef>(ma_format_f32, channels); }

    void DoUninit() override { _BufferRef.reset(); }

protected:
    inline void SetBufferData(const void *input, u32 frame_count) const {
        if (_BufferRef) _BufferRef->SetData(input, frame_count);
    }

    std::unique_ptr<BufferRef> _BufferRef;
};

// A `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref`.
struct SourceBufferNode : BufferRefNode {
    using BufferRefNode::BufferRefNode;

    ma_node *DoInit(ma_node_graph *graph) override {
        auto config = ma_data_source_node_config_init(_BufferRef->Get());
        int result = ma_data_source_node_init(graph, &config, nullptr, &SourceNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data source node: ", result));

        return &SourceNode;
    }

    void DoUninit() override {
        BufferRefNode::DoUninit();
        ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
    }

private:
    ma_data_source_node SourceNode;
};

// Wraps around (custom) `ma_data_passthrough_node`.
// Copies the input buffer in each callback to its internal `Buffer`.
struct PassthroughBufferNode : BufferRefNode {
    using BufferRefNode::BufferRefNode;

    ma_node *DoInit(ma_node_graph *graph) override {
        auto config = ma_data_passthrough_node_config_init(_BufferRef->Get());
        int result = ma_data_passthrough_node_init(graph, &config, nullptr, &PassthroughNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data passthrough node: ", result));

        return &PassthroughNode;
    }

    void DoUninit() override {
        BufferRefNode::DoUninit();
        ma_data_passthrough_node_uninit((ma_data_passthrough_node *)Node, nullptr);
    }

    void ReadBufferData(void *output, u32 frame_count) const noexcept {
        if (_BufferRef) _BufferRef->ReadData(output, frame_count);
    }

private:
    ma_data_passthrough_node PassthroughNode;
};

// A source node that owns an input device and copies the device callback input buffer to its own buffer.
struct DeviceInputNode : SourceBufferNode {
    DeviceInputNode(ComponentArgs &&args) : SourceBufferNode(std::move(args)) {
        Muted.Set_(true); // External input is muted by default.
        InputDevice = std::make_unique<AudioInputDevice>(ComponentArgs{this, "InputDevice"}, AudioInputCallback, this);
        InitBuffer(InputDevice->Channels);
    }

    static void AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *self = reinterpret_cast<DeviceInputNode *>(device->pUserData);
        self->SetBufferData(input, frame_count);

        (void)output;
    }

    bool AllowDelete() const override { return false; } // For now...

    std::unique_ptr<AudioInputDevice> InputDevice;
};

// A passthrough node that owns an output device.
// Must always be connected directly to the graph endpoint node.
// todo There must be a single "Master" `DeviceOutputNode`, which calls `ma_node_graph_read_pcm_frames`.
// Each remaining `DeviceOutputNode` will populate the device callback output buffer with its buffer data (`ReadBufferData`).
struct DeviceOutputNode : PassthroughBufferNode {
    DeviceOutputNode(ComponentArgs &&args) : PassthroughBufferNode(std::move(args)) {
        OutputDevice = std::make_unique<AudioOutputDevice>(ComponentArgs{this, "OutputDevice"}, AudioOutputCallback, this);
        InitBuffer(OutputDevice->Channels);
    }

    static void AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        if (Singleton) ma_node_graph_read_pcm_frames(Singleton->Get(), output, frame_count, nullptr);
        // auto *self = reinterpret_cast<DeviceOutputNode *>(device->pUserData);
        // self->ReadBufferData(output, frame_count);

        (void)device;
        (void)input;
    }

    bool AllowDelete() const override { return false; } // For now...

    // Always connects directly/only to the graph endpoint node.
    bool AllowOutputConnectionChange() const override { return false; }

    std::unique_ptr<AudioOutputDevice> OutputDevice;
};

// Wrapper around the graph endpoint node, which is allocated and managed by the MA graph.
// Technically, the graph endpoint node has an output bus, but it's handled specially by miniaudio.
// Most importantly, it is not possible to attach the graph endpoint's node into any other node.
// Thus, we treat it strictly as a sink and hide the fact that it technically has an output bus, since it functionally does not.
// The graph enforces that the only input is the "Master" `DeviceOutputNode` (hence not allowing dynamic input changes).
struct GraphEndpointNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    ma_node *DoInit(ma_node_graph *graph) override { return ma_node_graph_get_endpoint(graph); }

    bool AllowDelete() const override { return false; }

    // The graph endpoint node is completely hidden in the connection matrix.
    bool AllowInputConnectionChange() const override { return false; }
    bool AllowOutputConnectionChange() const override { return false; }
};

AudioGraph::AudioGraph(ComponentArgs &&args) : Component(std::move(args)) {
    Nodes.push_back(std::make_unique<DeviceInputNode>(ComponentArgs{this, "Input"}));
    Nodes.push_back(std::make_unique<DeviceOutputNode>(ComponentArgs{this, "Output"}));

    const auto *input_device = GetDeviceInputNode()->InputDevice.get();
    const auto *output_device = GetDeviceOutputNode()->OutputDevice.get();

    Graph = std::make_unique<MaGraph>(input_device->Channels);
    Nodes.push_back(std::make_unique<GraphEndpointNode>(ComponentArgs{this, GraphEndpointPathSegment}));

    Nodes.push_back(std::make_unique<FaustNode>(ComponentArgs{this, "Faust"}));
    Nodes.push_back(std::make_unique<WaveformNode>(ComponentArgs{this, "Waveform"}));
    for (const auto &node : Nodes) node->Init();

    const Field::References listened_fields = {
        input_device->On,
        input_device->Channels,
        input_device->SampleRate,
        input_device->Format,
        output_device->On,
        output_device->Channels,
        output_device->SampleRate,
        output_device->Format,
        Connections,
    };
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    for (const auto &node : Nodes) node->RegisterListener(this);

    // Set up default connections.
    // Connections.Connect(Nodes.Input.Id, Nodes.Faust.Id);
    // Connections.Connect(Nodes.Faust.Id, Nodes.Output.Id);
    Connections.Connect(GetDeviceInputNode()->Id, GetDeviceOutputNode()->Id);
    Connections.Connect(GetDeviceOutputNode()->Id, GetGraphEndpointNode()->Id);
    Singleton = this;
}

AudioGraph::~AudioGraph() {
    Singleton = nullptr;
    for (const auto &node : Nodes) node->UnregisterListener(this);
    Field::UnregisterChangeListener(this);
}
void AudioGraph::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::AudioGraph::DeleteNode &a) {},
    );
}

ma_node_graph *AudioGraph::Get() const { return Graph->Get(); }

// xxx depending on dynamic node positions is temporary.
DeviceInputNode *AudioGraph::GetDeviceInputNode() const { return static_cast<DeviceInputNode *>(Nodes[0].get()); }
DeviceOutputNode *AudioGraph::GetDeviceOutputNode() const { return static_cast<DeviceOutputNode *>(Nodes[1].get()); }
GraphEndpointNode *AudioGraph::GetGraphEndpointNode() const { return static_cast<GraphEndpointNode *>(Nodes[2].get()); }

void AudioGraph::OnFaustDspChanged(dsp *dsp) {
    // xxx depending on dynamic node positions is temporary.
    static_cast<FaustNode *>(Nodes[3].get())->OnFaustDspChanged(dsp);
    UpdateConnections();
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) { UpdateConnections(); }

void AudioGraph::OnFieldChanged() {
    const auto *input_device = GetDeviceInputNode()->InputDevice.get();
    const auto *output_device = GetDeviceOutputNode()->OutputDevice.get();
    if (input_device->On.IsChanged() ||
        input_device->Channels.IsChanged() ||
        input_device->Format.IsChanged() ||
        input_device->SampleRate.IsChanged() ||
        output_device->On.IsChanged() ||
        output_device->Channels.IsChanged() ||
        output_device->Format.IsChanged() ||
        output_device->SampleRate.IsChanged()) {
        for (const auto &node : Nodes) node->Uninit();
        Graph = std::make_unique<MaGraph>(input_device->Channels);
        for (const auto &node : Nodes) node->Init();
        UpdateConnections();
        return;
    }

    // if (input_device->SampleRate.IsChanged() || output_device->SampleRate.IsChanged()) {
    //     for (const auto &node : Nodes) node->OnDeviceSampleRateChanged();
    // }

    if (Connections.IsChanged()) {
        UpdateConnections();
    }
}

u32 AudioGraph::GetDeviceSampleRate() const { return GetDeviceOutputNode()->OutputDevice->SampleRate; }
// u64 AudioGraph::GetDeviceBufferSize() const { return GetDeviceOutputNode()->GetBufferSize(); }
u64 AudioGraph::GetDeviceBufferSize() const { return GetDeviceOutputNode()->OutputDevice->Get()->playback.internalPeriodSizeInFrames; }

void AudioGraph::UpdateConnections() {
    for (const auto &node : Nodes) {
        if (node->AllowInputConnectionChange() && node->AllowOutputConnectionChange()) {
            node->DisconnectAll();
        }
    }

    for (const auto &out_node : Nodes) {
        for (const auto &in_node : Nodes) {
            if (Connections.IsConnected(out_node->Id, in_node->Id)) {
                out_node->ConnectTo(*in_node);
            }
        }
    }

    // Update node active states.
    // Nodes that are turned off (here - disabled) are not removed from the `Connections` object in order to preserve their connections.
    // So we need to check if there is a path to the output node that doesn't go through any disabled nodes.
    for (const auto &node : Nodes) {
        node->SetActive(GetGraphEndpointNode() != nullptr && Connections.HasPath(node->Id, GetGraphEndpointNode()->Id));
    }
}

using namespace ImGui;

void AudioGraph::RenderConnections() const {
    // Calculate the maximum I/O label widths.
    ImVec2 max_label_w_no_padding{0, 0}; // I/O vec: in (left), out (top)
    for (const auto &node : Nodes) {
        const float label_w = CalcTextSize(node->Name.c_str()).x;
        if (node->CanConnectInput()) max_label_w_no_padding.x = std::max(max_label_w_no_padding.x, label_w);
        if (node->CanConnectOutput()) max_label_w_no_padding.y = std::max(max_label_w_no_padding.y, label_w);
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
        if (!out_node->CanConnectOutput()) continue;

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
        if (!in_node->CanConnectInput()) continue;

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
            if (!out_node->CanConnectOutput()) continue;

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
    const auto *input_device = GetDeviceInputNode()->InputDevice.get();
    const auto *output_device = GetDeviceOutputNode()->OutputDevice.get();
    if (BeginTabItem(input_device->ImGuiLabel.c_str())) {
        input_device->Draw();
        EndTabItem();
    }
    if (BeginTabItem(output_device->ImGuiLabel.c_str())) {
        output_device->Draw();
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
