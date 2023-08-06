#include "AudioGraph.h"

#include <concepts>
#include <set>

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "miniaudio.h"

#include "Core/Container/AdjacencyListAction.h"
#include "Project/Audio/Device/AudioDevice.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/WaveformNode.h"

#include "ma_data_passthrough_node/ma_data_passthrough_node.h"

#define ma_offset_ptr(p, offset) (((ma_uint8 *)(p)) + (offset))

// Wraps around `ma_node_graph`.
struct MaGraph {
    MaGraph(u32 channels) {
        auto config = ma_node_graph_config_init(channels);
        _Graph = std::make_unique<ma_node_graph>();
        ma_result result = ma_node_graph_init(&config, nullptr, _Graph.get());
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", int(result)));
    }
    ~MaGraph() {
        ma_node_graph_uninit(_Graph.get(), nullptr);
        _Graph.reset();
    }

    inline ma_node_graph *Get() const noexcept { return _Graph.get(); }

private:
    std::unique_ptr<ma_node_graph> _Graph;
};

struct BufferRef {
    BufferRef(ma_format format, u32 channels) {
        ma_result result = ma_audio_buffer_ref_init(format, channels, nullptr, 0, &Ref);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize buffer ref: ", int(result)));
    }
    ~BufferRef() {
        ma_audio_buffer_ref_uninit(&Ref);
    }

    // u64 GetSize() const { return Ref.sizeInFrames; }

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

    inline void InitBuffer(u32 channels) { _BufferRef = std::make_unique<BufferRef>(ma_format_f32, channels); }

protected:
    inline void SetBufferData(const void *input, u32 frame_count) const {
        if (_BufferRef) _BufferRef->SetData(input, frame_count);
    }

    std::unique_ptr<BufferRef> _BufferRef;
};

// A `ma_data_source_node` whose `ma_data_source` is a `ma_duplex_rb`.
// A source node that owns an input device and copies the device callback input buffer to a ring buffer.
struct InputDeviceNode : AudioGraphNode {
    InputDeviceNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
        Device = std::make_unique<AudioDevice>(ComponentArgs{this, "InputDevice"}, IO_In, Graph->SampleRate, AudioInputCallback, this);

        ma_device *device = Device->Get();

        // This min/max SR approach seems to work to get both upsampling and downsampling
        // (from low device SR to high client SR and vice versa), but it doesn't seem like the best approach.
        const auto [sr_min, sr_max] = std::minmax(device->capture.internalSampleRate, device->sampleRate);
        ma_result result = ma_duplex_rb_init(device->capture.format, device->capture.channels, sr_max, sr_min, device->capture.internalPeriodSizeInFrames, &device->pContext->allocationCallbacks, &DuplexRb);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize ring buffer: ", int(result)));

        auto config = ma_data_source_node_config_init(&DuplexRb);
        result = ma_data_source_node_init(Graph->Get(), &config, nullptr, &SourceNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data source node: ", int(result)));

        Node = &SourceNode;

        UpdateAll();
    }

    ~InputDeviceNode() {
        ma_duplex_rb_uninit(&DuplexRb);
        ma_data_source_node_uninit(&SourceNode, nullptr);
        Node = nullptr;
    }

    string GetTreeLabel() const override { return Device->GetFullLabel(); }

    // Adapted from `ma_device__handle_duplex_callback_capture`.
    static void AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *user_data = reinterpret_cast<AudioDevice::UserData *>(device->pUserData);
        auto *self = reinterpret_cast<InputDeviceNode *>(user_data->User);
        ma_duplex_rb *duplex_rb = &self->DuplexRb;

        ma_uint32 total_frames_processed = 0;
        const void *running_frames = input;
        for (;;) {
            ma_uint32 frames_to_process = frame_count - total_frames_processed;
            ma_uint64 frames_processed;
            void *frames;
            ma_result result = ma_pcm_rb_acquire_write(&duplex_rb->rb, &frames_to_process, &frames);
            if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to acquire write pointer for capture ring buffer: {}", int(result)));

            if (frames_to_process == 0) {
                if (ma_pcm_rb_pointer_distance(&duplex_rb->rb) == (ma_int32)ma_pcm_rb_get_subbuffer_size(&duplex_rb->rb)) {
                    break; // Overrun. Not enough room in the ring buffer for input frame. Excess frames are dropped.
                }
            }
            ma_copy_pcm_frames(frames, running_frames, frames_to_process, ma_format_f32, device->capture.channels);

            frames_processed = frames_to_process;
            result = ma_pcm_rb_commit_write(&duplex_rb->rb, (ma_uint32)frames_processed);
            if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to commit capture PCM frames to ring buffer: {}", int(result)));

            running_frames = ma_offset_ptr(running_frames, frames_processed * ma_get_bytes_per_frame(device->capture.internalFormat, device->capture.internalChannels));
            total_frames_processed += (ma_uint32)frames_processed;

            if (frames_processed == 0) break; /* Done. */
        }

        (void)output;
    }

    void OnSampleRateChanged() override {
        AudioGraphNode::OnSampleRateChanged();
        Device->SetClientSampleRate(Graph->SampleRate);
    }

    std::unique_ptr<AudioDevice> Device;
    ma_duplex_rb DuplexRb;
    ma_data_source_node SourceNode;

private:
    void Render() const override {
        Device->Draw();
        ImGui::Spacing();
        AudioGraphNode::Render();
    }
};

// Wraps around (custom) `ma_data_passthrough_node`.
// Copies the input buffer in each callback to its internal `Buffer`.
struct PassthroughBufferNode : BufferRefNode {
    PassthroughBufferNode(ComponentArgs &&args) : BufferRefNode(std::move(args)) {
        InitBuffer(1); // todo inline `PassthroughBufferNode` into `OutputDeviceNode` and initialize the buffer _after_ the device to get its channel count.
        auto config = ma_data_passthrough_node_config_init(_BufferRef->Get());
        ma_result result = ma_data_passthrough_node_init(Graph->Get(), &config, nullptr, &PassthroughNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data passthrough node: ", int(result)));
        Node = &PassthroughNode;
    }

    ~PassthroughBufferNode() {
        ma_data_passthrough_node_uninit((ma_data_passthrough_node *)Node, nullptr);
        Node = nullptr;
    }

    void ReadBufferData(void *output, u32 frame_count) const noexcept {
        if (_BufferRef) _BufferRef->ReadData(output, frame_count);
    }

private:
    ma_data_passthrough_node PassthroughNode;
};

/*
`OutputDeviceNode` is a passthrough node that owns an output device.
Whenever there is at least one output device node, there is a single "primary" output device node.
The primary output device node passes the graph endpoint node's output buffer to the device callback output buffer.
Each remaining output device node populates its device callback output buffer with its buffer data (`ReadBufferData`).

It is up to the owning graph to ensure that _each_ of its output device nodes is always connected directly to the graph endpoint node.
*/
struct OutputDeviceNode : PassthroughBufferNode {
    inline static OutputDeviceNode *Primary;
    inline static std::set<OutputDeviceNode *> All;

    OutputDeviceNode(ComponentArgs &&args) : PassthroughBufferNode(std::move(args)) {
        Device = std::make_unique<AudioDevice>(ComponentArgs{this, "OutputDevice"}, IO_Out, Graph->SampleRate, AudioOutputCallback, this);
        UpdateAll();
        if (!Primary) Primary = this;
        All.insert(this);
    }
    ~OutputDeviceNode() {
        All.erase(this);
        if (Primary == this) {
            Primary = All.empty() ? nullptr : *All.begin();
        }
    }

    string GetTreeLabel() const override { return Device->GetFullLabel(); }

    static void AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *user_data = reinterpret_cast<AudioDevice::UserData *>(device->pUserData);
        auto *self = reinterpret_cast<OutputDeviceNode *>(user_data->User);
        if (self == Primary && self->Graph) {
            ma_node_graph_read_pcm_frames(self->Graph->Get(), output, frame_count, nullptr);
        } else {
            // Every output device node is connected directly into the graph endpoint node.
            // After the primary output device node has pulled from the graph endpoint node,
            // This secondary output device node will have its input busses mixed and copied into its passthrough buffer.
            // Here, we forward these buffer frames to this device node's owned output audio device.
            // todo audio graph should only connect the primary device always, and only connect each secondary devices to the graph endpoint
            // when it has any input nodes. Currently, we get the last recorded frame looped to the owned output device.
            self->ReadBufferData(output, frame_count);
        }

        (void)device;
        (void)input;
    }

    // Always connects directly/only to the graph endpoint node.
    bool AllowOutputConnectionChange() const override { return false; }

    void OnSampleRateChanged() override {
        PassthroughBufferNode::OnSampleRateChanged();
        Device->SetClientSampleRate(Graph->SampleRate);
    }

    std::unique_ptr<AudioDevice> Device;

private:
    void Render() const override {
        Device->Draw();
        ImGui::Spacing();
        AudioGraphNode::Render();
    }
};

AudioGraph::AudioGraph(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Graph = std::make_unique<MaGraph>(1);
    Node = ma_node_graph_get_endpoint(Graph->Get());
    IsActive = true; // The graph is always active, since it is always connected to itself.
    UpdateAll();
    this->RegisterListener(this); // The graph listens to itself _as an audio graph node_.

    Nodes.EmplaceBack_(InputDeviceNodeTypeId);
    Nodes.back()->SetMuted(true); // External input is muted by default.
    Nodes.EmplaceBack_(OutputDeviceNodeTypeId);

    if (SampleRate == 0u) SampleRate.Set_(GetDefaultSampleRate());
    Nodes.EmplaceBack_(WaveformNodeTypeId);

    const Field::References listened_fields = {Nodes, Connections};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);

    // Set up default connections.
    // Note that the device output -> graph endpoint node connection is handled during device output node initialization.
    Connections.Connect(GetInputDeviceNodes().front()->Id, GetOutputDeviceNodes().front()->Id);
}

AudioGraph::~AudioGraph() {
    Nodes.Clear();
    Field::UnregisterChangeListener(this);
}

template<std::derived_from<AudioGraphNode> AudioGraphNodeSubType>
static std::unique_ptr<AudioGraphNodeSubType> CreateNode(AudioGraph *graph, Component::ComponentArgs &&args) {
    auto node = std::make_unique<AudioGraphNodeSubType>(std::move(args));
    node->RegisterListener(graph);
    if (const auto *device_output_node = dynamic_cast<OutputDeviceNode *>(node.get())) {
        graph->Connections.Connect(device_output_node->Id, graph->Id);
    }
    return node;
}

std::unique_ptr<AudioGraphNode> AudioGraph::CreateNode(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    ComponentArgs args{parent, path_segment, "", path_prefix_segment};
    auto *graph = static_cast<AudioGraph *>(parent->Parent);
    if (path_segment == InputDeviceNodeTypeId) return CreateNode<InputDeviceNode>(graph, std::move(args));
    if (path_segment == OutputDeviceNodeTypeId) return CreateNode<OutputDeviceNode>(graph, std::move(args));
    if (path_segment == WaveformNodeTypeId) return CreateNode<WaveformNode>(graph, std::move(args));
    if (path_segment == FaustNodeTypeId) return CreateNode<FaustNode>(graph, std::move(args));

    return nullptr;
}

void AudioGraph::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::AudioGraph::CreateNode &a) {
            Nodes.EmplaceBack(a.node_type_id);
        },
        [this](const Action::AudioGraph::DeleteNode &a) {
            Nodes.EraseId(a.id);
            Connections.DisconnectAll(a.id);
        },
        [](const Action::AudioGraph::SetDeviceDataFormat &a) {
            if (!Component::ById.contains(a.id)) throw std::runtime_error(std::format("No audio device with id {} exists.", a.id));

            auto *data_format = static_cast<const AudioDevice::DataFormat *>(Component::ById.at(a.id));
            data_format->SampleFormat.Set(a.sample_format);
            data_format->SampleRate.Set(a.sample_rate);
            data_format->Channels.Set(a.channels);
        },
    );
}

ma_node_graph *AudioGraph::Get() const { return Graph->Get(); }
dsp *AudioGraph::GetFaustDsp() const { return FaustDsp; }

// A sample rate is considered "native" by the graph (and suffixed with an asterix)
// if it is native to all device nodes within the graph (or if there are no device nodes in the graph).
bool AudioGraph::IsNativeSampleRate(u32 sample_rate) const {
    for (const auto *device_node : GetInputDeviceNodes()) {
        if (!device_node->Device->IsNativeSampleRate(sample_rate)) return false;
    }
    for (const auto *device_node : GetOutputDeviceNodes()) {
        if (!device_node->Device->IsNativeSampleRate(sample_rate)) return false;
    }
    return true;
}

// Returns the highest-priority sample rate (see `AudioDevice::PrioritizedSampleRates`) natively supported by all device nodes in this graph,
// or the highest-priority sample rate supported by any device node if none are natively supported by all device nodes.
u32 AudioGraph::GetDefaultSampleRate() const {
    for (const u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
        if (IsNativeSampleRate(sample_rate)) return sample_rate;
    }
    for (const u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
        // Favor doing sample rate conversion on input rather than output.
        for (const auto *device_node : GetOutputDeviceNodes()) {
            if (device_node->Device->IsNativeSampleRate(sample_rate)) return sample_rate;
        }
        for (const auto *device_node : GetInputDeviceNodes()) {
            if (device_node->Device->IsNativeSampleRate(sample_rate)) return sample_rate;
        }
    }
    return AudioDevice::PrioritizedSampleRates.front();
}

std::string AudioGraph::GetSampleRateName(u32 sample_rate) const {
    return std::format("{}{}", to_string(sample_rate), IsNativeSampleRate(sample_rate) ? "*" : "");
}

void AudioGraph::OnFaustDspChanged(dsp *dsp) {
    const auto *faust_node = FindByPathSegment(FaustNodeTypeId);
    FaustDsp = dsp;
    if (!dsp && faust_node) {
        Nodes.EraseId(faust_node->Id);
    } else if (dsp) {
        if (faust_node) Nodes.EraseId(faust_node->Id);
        Nodes.EmplaceBack_(FaustNodeTypeId);
    }
    UpdateConnections(); // todo only update connections if the dsp change caused a change in the number of channels.
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) { UpdateConnections(); }

void AudioGraph::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();

    if (Nodes.IsChanged() || Connections.IsChanged()) {
        UpdateConnections();
    }
}

std::unordered_set<AudioGraphNode *> AudioGraph::GetSourceNodes(const AudioGraphNode *node) const {
    std::unordered_set<AudioGraphNode *> nodes;
    for (auto *other_node : Nodes) {
        if (other_node == node) continue;
        if (Connections.IsConnected(other_node->Id, node->Id)) {
            nodes.insert(other_node);
        }
    }
    return nodes;
}

std::unordered_set<AudioGraphNode *> AudioGraph::GetDestinationNodes(const AudioGraphNode *node) const {
    std::unordered_set<AudioGraphNode *> nodes;
    for (auto *other_node : Nodes) {
        if (other_node == node) continue;
        if (Connections.IsConnected(node->Id, other_node->Id)) {
            nodes.insert(other_node);
        }
    }
    return nodes;
}

void AudioGraph::UpdateConnections() {
    for (auto *node : Nodes) {
        node->DisconnectAll();
    }

    // The graph does not keep itself in its `Nodes` list, so we must connect it manually.
    for (auto *device_output_node : GetOutputDeviceNodes()) {
        device_output_node->ConnectTo(*this);
    }

    for (auto *out_node : Nodes) {
        for (auto *in_node : Nodes) {
            if (Connections.IsConnected(out_node->Id, in_node->Id)) {
                out_node->ConnectTo(*in_node);
            }
        }
    }

    for (auto *node : Nodes) node->SetActive(Connections.HasPath(node->Id, Id));
}

using namespace ImGui;

static void RenderConnectionsLabelFrame(InteractionFlags interaction_flags) {
    const auto fill_color =
        interaction_flags & InteractionFlags_Held ?
        ImGuiCol_ButtonActive :
        interaction_flags & InteractionFlags_Hovered ?
        ImGuiCol_ButtonHovered :
        ImGuiCol_WindowBg;
    RenderFrame(GetItemRectMin(), GetItemRectMax(), GetColorU32(fill_color));
}

// todo move more label logic into this method.
static void RenderConnectionsLabel(IO io, const AudioGraphNode *node, const string &ellipsified_label, InteractionFlags interaction_flags) {
    const auto *graph = node->Graph;
    const bool is_active = node->IsActive;
    if (!is_active) BeginDisabled();
    RenderConnectionsLabelFrame(interaction_flags);
    if (io == IO_Out) {
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            GetCursorScreenPos() + ImVec2{(GetItemRectSize().x - GetTextLineHeight()) / 2, GetItemRectSize().y - ImGui::GetStyle().FramePadding.y},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
    } else {
        TextUnformatted(ellipsified_label.c_str());
    }
    if (!is_active) EndDisabled();

    const bool text_clipped = ellipsified_label.ends_with("...");

    if (text_clipped && (interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", node->Name.c_str());
    if (interaction_flags & InteractionFlags_Clicked) {
        if (graph->Focus()) graph->SelectedNodeId = node->Id;
    }
}

void AudioGraph::Connections::Render() const {
    const auto &graph = *static_cast<const AudioGraph *>(Parent);
    using IoVec = ImVec2; // A 2D vector representing floats corresponding to input/output, rather than an x/y position.

    const auto &style = graph.Style.Matrix;

    // Calculate max I/O label widths.
    IoVec max_label_w_no_padding{0, 0};
    for (const auto *node : graph.Nodes) {
        const float label_w = CalcTextSize(node->Name.c_str()).x;
        if (node->CanConnectInput()) max_label_w_no_padding.x = std::max(max_label_w_no_padding.x, label_w);
        if (node->CanConnectOutput()) max_label_w_no_padding.y = std::max(max_label_w_no_padding.y, label_w);
    }

    const ImVec2 label_padding = ImVec2{ImGui::GetStyle().ItemInnerSpacing.x, 0} + ImGui::GetStyle().FramePadding;
    const float max_allowed_label_w = style.MaxLabelSpace * GetTextLineHeight();
    const IoVec node_label_w_no_padding = {std::min(max_allowed_label_w, max_label_w_no_padding.x), std::min(max_allowed_label_w, max_label_w_no_padding.y)};
    const IoVec node_label_w = node_label_w_no_padding + label_padding.x * 2;
    const float fhws = GetFrameHeightWithSpacing();
    const auto og_cursor_pos = GetCursorScreenPos();
    const ImVec2 grid_top_left = og_cursor_pos + node_label_w + fhws; // Last line-height is for the I/O header labels.

    BeginGroup();

    // I/O header frames + labels on the left/top, respectively.
    static const string InputsLabel = "Inputs", OutputsLabel = "Outputs";
    const IoVec io_header_w_no_padding = ImVec2{CalcTextSize(InputsLabel).x, CalcTextSize(OutputsLabel).x};
    const IoVec io_header_w = io_header_w_no_padding + label_padding.x * 2;
    IoVec io_frame_w = GetContentRegionAvail() - (node_label_w + fhws);
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
    for (const auto *out_node : graph.Nodes) {
        if (!out_node->CanConnectOutput()) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_count, -node_label_w.y});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, node_label_w.y}, std::format("{}:{}", out_node->ImGuiLabel, to_string(IO_Out)).c_str());
        const string label = out_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.y);
        RenderConnectionsLabel(IO_Out, out_node, ellipsified_label, label_interaction_flags);
        out_count++;
    }

    // Input channel labels and mixer cells.
    u32 in_i = 0;
    for (const auto *in_node : graph.Nodes) {
        if (!in_node->CanConnectInput()) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{-node_label_w.x, (cell_size + cell_gap) * in_i});
        const auto label_interaction_flags = fg::InvisibleButton({node_label_w.x, cell_size}, std::format("{}:{}", in_node->ImGuiLabel, to_string(IO_In)).c_str());
        const string label = in_node->Name;
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.x);
        SetCursorPos(GetCursorPos() + ImVec2{node_label_w.x - CalcTextSize(ellipsified_label.c_str()).x - label_padding.y, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.
        RenderConnectionsLabel(IO_In, in_node, ellipsified_label, label_interaction_flags);

        u32 out_i = 0;
        for (const auto *out_node : graph.Nodes) {
            if (!out_node->CanConnectOutput()) continue;

            PushID(in_i * out_count + out_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * out_i, (cell_size + cell_gap) * in_i});

            const bool disabled = out_node->Id == in_node->Id;
            if (disabled) BeginDisabled();

            const auto cell_interaction_flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (cell_interaction_flags & InteractionFlags_Clicked) {
                Action::AdjacencyList::ToggleConnection{Path, out_node->Id, in_node->Id}.q();
            }

            const bool is_connected = IsConnected(out_node->Id, in_node->Id);
            const auto fill_color =
                cell_interaction_flags & InteractionFlags_Held ?
                ImGuiCol_ButtonActive :
                (cell_interaction_flags & InteractionFlags_Hovered ?
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

std::optional<string> AudioGraph::RenderNodeCreateSelector() const {
    std::optional<string> node_type_id;
    if (ImGui::TreeNode("Create")) {
        if (ImGui::TreeNode("Device")) {
            // Multiple input/output devices nodes should work in principle, but it's tricky to get right,
            // and let's be honest - it's a niche use case anyway.
            if (Button(InputDeviceNodeTypeId.c_str())) node_type_id = InputDeviceNodeTypeId;
            // SameLine();
            // if (Button(OutputDeviceNodeTypeId.c_str())) node_type_id = OutputDeviceNodeTypeId;
            TreePop();
        }
        if (ImGui::TreeNode("Generator")) {
            if (Button(WaveformNodeTypeId.c_str())) node_type_id = WaveformNodeTypeId;
            TreePop();
        }
        // todo miniaudio effects
        // if (ImGui::TreeNode("Effect")) {
        //     TreePop();
        // }
        // More work is needed before we can handle multiple Faust nodes.
        // if (ImGui::TreeNode(FaustNodeTypeId.c_str())) {
        //     if (Button("Custom")) node_type_id = FaustNodeTypeId;
        //     TreePop();
        // }
        TreePop();
    }
    return node_type_id;
}

void AudioGraph::Render() const {
    SampleRate.Render(AudioDevice::PrioritizedSampleRates);
    AudioGraphNode::Render();

    if (SelectedNodeId != 0) {
        SetNextItemOpen(true);
        if (IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
    }

    if (ImGui::TreeNode(Nodes.ImGuiLabel.c_str())) {
        if (const auto new_node_type_id = RenderNodeCreateSelector()) {
            Action::AudioGraph::CreateNode{*new_node_type_id}.q();
        }

        for (const auto *node : Nodes) {
            if (SelectedNodeId != 0) {
                const bool is_node_selected = SelectedNodeId == node->Id;
                SetNextItemOpen(is_node_selected);
                if (is_node_selected && IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
            }
            const bool node_active = node->IsActive;
            // Similar to `ImGuiCol_TextDisabled`, but a bit lighter.
            if (!node_active) PushStyleColor(ImGuiCol_Text, {0.7f, 0.7f, 0.7f, 1.0f});
            const bool node_open = ImGui::TreeNode(node->ImGuiLabel.c_str(), "%s", node->GetTreeLabel().c_str());
            if (!node_active) PopStyleColor();
            if (node_open) {
                if (Button("Delete")) Action::AudioGraph::DeleteNode{node->Id}.q();
                node->Draw();
                TreePop();
            }
        }
        TreePop();
    }

    SelectedNodeId = 0;
}
