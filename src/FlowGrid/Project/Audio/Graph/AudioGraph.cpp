#include "AudioGraph.h"

#include <concepts>

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "miniaudio.h"

#include "Core/Container/AdjacencyListAction.h"
#include "Project/Audio/AudioDevice.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/WaveformNode.h"

#include "ma_data_passthrough_node/ma_data_passthrough_node.h"

static const AudioGraph *Singleton;

// Wraps around `ma_node_graph`.
struct MaGraph {
    MaGraph(u32 channels) {
        auto config = ma_node_graph_config_init(channels);
        _Graph = std::make_unique<ma_node_graph>();
        const int result = ma_node_graph_init(&config, nullptr, _Graph.get());

        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));
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

protected:
    inline void SetBufferData(const void *input, u32 frame_count) const {
        if (_BufferRef) _BufferRef->SetData(input, frame_count);
    }

    std::unique_ptr<BufferRef> _BufferRef;
};

// A `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref`.
struct SourceBufferNode : BufferRefNode {
    SourceBufferNode(ComponentArgs &&args) : BufferRefNode(std::move(args)) {
        InitBuffer(1);
        auto config = ma_data_source_node_config_init(_BufferRef->Get());
        int result = ma_data_source_node_init(Graph->Get(), &config, nullptr, &SourceNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data source node: ", result));

        Node = &SourceNode;
    }
    ~SourceBufferNode() {
        ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
        Node = nullptr;
    }

private:
    ma_data_source_node SourceNode;
};

// Wraps around (custom) `ma_data_passthrough_node`.
// Copies the input buffer in each callback to its internal `Buffer`.
struct PassthroughBufferNode : BufferRefNode {
    PassthroughBufferNode(ComponentArgs &&args) : BufferRefNode(std::move(args)) {
        InitBuffer(1);
        auto config = ma_data_passthrough_node_config_init(_BufferRef->Get());
        int result = ma_data_passthrough_node_init(Graph->Get(), &config, nullptr, &PassthroughNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data passthrough node: ", result));
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

// A source node that owns an input device and copies the device callback input buffer to its own buffer.
struct DeviceInputNode : SourceBufferNode {
    DeviceInputNode(ComponentArgs &&args) : SourceBufferNode(std::move(args)) {
        Device = std::make_unique<AudioDevice>(ComponentArgs{this, "InputDevice"}, IO_In, AudioInputCallback, this);
        UpdateAll();
    }

    static void AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *self = reinterpret_cast<DeviceInputNode *>(device->pUserData);
        self->SetBufferData(input, frame_count);

        (void)output;
    }

    std::unique_ptr<AudioDevice> Device;

private:
    void Render() const override {
        AudioGraphNode::Render();

        ImGui::Spacing();
        Device->Draw();
    }
};

// A passthrough node that owns an output device.
// Must always be connected directly to the graph endpoint node.
// todo There must be a single "Master" `DeviceOutputNode`, which calls `ma_node_graph_read_pcm_frames`.
// Each remaining `DeviceOutputNode` will populate the device callback output buffer with its buffer data (`ReadBufferData`).
struct DeviceOutputNode : PassthroughBufferNode {
    DeviceOutputNode(ComponentArgs &&args) : PassthroughBufferNode(std::move(args)) {
        Device = std::make_unique<AudioDevice>(ComponentArgs{this, "OutputDevice"}, IO_Out, AudioOutputCallback, this);
        UpdateAll();
    }

    static void AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        if (Singleton) ma_node_graph_read_pcm_frames(Singleton->Get(), output, frame_count, nullptr);
        // auto *self = reinterpret_cast<DeviceOutputNode *>(device->pUserData);
        // self->ReadBufferData(output, frame_count);

        (void)device;
        (void)input;
    }

    // Always connects directly/only to the graph endpoint node.
    bool AllowOutputConnectionChange() const override { return false; }

    std::unique_ptr<AudioDevice> Device;

private:
    void Render() const override {
        AudioGraphNode::Render();

        ImGui::Spacing();
        Device->Draw();
    }
};

static const string DeviceInputPathSegment = "Input";
static const string DeviceOutputPathSegment = "Output";
static const string WaveformPathSegment = "Waveform";
static const string FaustPathSegment = "Faust";

AudioGraph::AudioGraph(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Graph = std::make_unique<MaGraph>(1);
    Node = ma_node_graph_get_endpoint(Graph->Get());
    IsActive = true; // The graph is always active, since it is always connected to itself.
    UpdateAll();
    this->RegisterListener(this); // The graph listens to itself _as an audio graph node_.

    Nodes.EmplaceBack(DeviceInputPathSegment);
    Nodes.back()->SetMuted(true); // External input is muted by default.
    Nodes.EmplaceBack(DeviceOutputPathSegment);
    Nodes.EmplaceBack(WaveformPathSegment);

    const Field::References listened_fields = {Nodes, Connections};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);

    // Set up default connections.
    // Note that the device output -> graph endpoint node connection is handled during device output node initialization.
    Connections.Connect(GetDeviceInputNode()->Id, GetDeviceOutputNode()->Id);
    Singleton = this;
}

AudioGraph::~AudioGraph() {
    Nodes.Clear();
    Singleton = nullptr;
    Field::UnregisterChangeListener(this);
}

template<std::derived_from<AudioGraphNode> AudioGraphNodeSubType>
static std::unique_ptr<AudioGraphNodeSubType> CreateNode(AudioGraph *graph, Component::ComponentArgs &&args) {
    auto node = std::make_unique<AudioGraphNodeSubType>(std::move(args));
    node->RegisterListener(graph);
    AudioDevice *device = nullptr;
    if (const auto *device_input_node = dynamic_cast<DeviceInputNode *>(node.get())) {
        device = device_input_node->Device.get();
    } else if (const auto *device_output_node = dynamic_cast<DeviceOutputNode *>(node.get())) {
        device = device_output_node->Device.get();
        graph->Connections.Connect(device_output_node->Id, graph->Id);
    }
    if (device) {
        const Field::References listened_fields = {device->Channels, device->SampleRate, device->Format};
        for (const Field &field : listened_fields) field.RegisterChangeListener(graph);
    }
    return node;
}

std::unique_ptr<AudioGraphNode> AudioGraph::CreateNode(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    ComponentArgs args{parent, path_segment, "", path_prefix_segment};
    auto *graph = static_cast<AudioGraph *>(parent->Parent);
    if (path_segment == DeviceInputPathSegment) return CreateNode<DeviceInputNode>(graph, std::move(args));
    if (path_segment == DeviceOutputPathSegment) return CreateNode<DeviceOutputNode>(graph, std::move(args));
    if (path_segment == WaveformPathSegment) return CreateNode<WaveformNode>(graph, std::move(args));
    if (path_segment == FaustPathSegment) return CreateNode<FaustNode>(graph, std::move(args));

    return nullptr;
}

void AudioGraph::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::AudioGraph::DeleteNode &a) {
            Nodes.EraseId(a.id);
            Connections.DisconnectAll(a.id);
        },
    );
}

ma_node_graph *AudioGraph::Get() const { return Graph->Get(); }
dsp *AudioGraph::GetFaustDsp() const { return FaustDsp; }

AudioGraphNode *AudioGraph::FindByPathSegment(string_view path_segment) const {
    auto node_it = std::find_if(Nodes.begin(), Nodes.end(), [path_segment](const auto *node) { return node->PathSegment == path_segment; });
    return node_it != Nodes.end() ? node_it->get() : nullptr;
}

DeviceInputNode *AudioGraph::GetDeviceInputNode() const { return static_cast<DeviceInputNode *>(FindByPathSegment(DeviceInputPathSegment)); }
DeviceOutputNode *AudioGraph::GetDeviceOutputNode() const { return static_cast<DeviceOutputNode *>(FindByPathSegment(DeviceOutputPathSegment)); }

void AudioGraph::OnFaustDspChanged(dsp *dsp) {
    const auto *faust_node = FindByPathSegment(FaustPathSegment);
    FaustDsp = dsp;
    if (!dsp && faust_node) {
        Nodes.EraseId(faust_node->Id);
    } else if (dsp) {
        if (faust_node) Nodes.EraseId(faust_node->Id);
        Nodes.EmplaceBack(FaustPathSegment);
    }
    UpdateConnections(); // todo only update connections if the dsp change caused a change in the number of channels.
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) { UpdateConnections(); }

void AudioGraph::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();

    if (const auto *device_node = GetDeviceOutputNode()) {
        const auto *device = device_node->Device.get();
        if (device->Channels.IsChanged() || device->Format.IsChanged()) {
            // todo
        }
        if (device->SampleRate.IsChanged()) {
            for (auto *node : Nodes) node->OnSampleRateChanged();
        }
    }
    if (const auto *device_node = GetDeviceInputNode()) {
        const auto *device = device_node->Device.get();
        if (device->Channels.IsChanged() || device->Format.IsChanged()) {
            // todo
        }
        if (device->SampleRate.IsChanged()) {
            for (auto *node : Nodes) node->OnSampleRateChanged();
        }
    }

    if (Nodes.IsChanged() || Connections.IsChanged()) {
        UpdateConnections();
    }
}

u32 AudioGraph::GetSampleRate() const {
    if (const auto *device_output_node = GetDeviceOutputNode()) return device_output_node->Device->SampleRate;
    if (const auto *device_input_node = GetDeviceInputNode()) return device_input_node->Device->SampleRate;
    return 0;
}

u64 AudioGraph::GetBufferSize() const {
    if (const auto *device_output_node = GetDeviceOutputNode()) return device_output_node->Device->GetBufferSize();
    return 0;
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
    if (auto *device_output_node = GetDeviceOutputNode()) device_output_node->ConnectTo(*this);

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

void AudioGraph::RenderConnections() const {
    // Calculate the maximum I/O label widths.
    ImVec2 max_label_w_no_padding{0, 0}; // I/O vec: in (left), out (top)
    for (const auto *node : Nodes) {
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
    for (const auto *out_node : Nodes) {
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
    for (const auto *in_node : Nodes) {
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
        for (const auto *out_node : Nodes) {
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

void AudioGraph::Render() const {
    if (BeginTabItem("Nodes")) {
        if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            AudioGraphNode::Render();
            for (const auto *node : Nodes) {
                if (TreeNodeEx(node->ImGuiLabel.c_str())) {
                    node->Draw();
                    TreePop();
                }
            }
            EndTabItem();
        }
        EndTabItem();
    }
    if (BeginTabItem(Connections.ImGuiLabel.c_str())) {
        RenderConnections();
        EndTabItem();
    }
}
