#include "AudioGraph.h"

#include <range/v3/range/conversion.hpp>

#include "Core/Container/AdjacencyListAction.h"
#include "Core/Primitive/String.h"
#include "Helper/String.h"
#include "Project/Audio/Device/AudioDevice.h"
#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/WaveformNode.h"
#include "UI/HelpMarker.h"
#include "UI/InvisibleButton.h"
#include "UI/Styling.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include "ma_channel_converter_node/ma_channel_converter_node.h"
#include "ma_data_passthrough_node/ma_data_passthrough_node.h"

#define ma_offset_ptr(p, offset) (((ma_uint8 *)(p)) + (offset))

using namespace ImGui;

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

struct DeviceMaNode : MaNode {
    DeviceMaNode(
        IO type,
        AudioDevice::AudioCallback &&callback,
        AudioDevice::TargetConfig &&target_config,
        const void *client_user_data
    ) : Device(type, std::move(callback), std::move(target_config), client_user_data) {}

    virtual ~DeviceMaNode() {}

    inline void UpdateDeviceConfig(AudioDevice::TargetConfig &&target_config) { Device.SetConfig(std::move(target_config)); }

    AudioDevice Device;
};

struct DeviceNode : AudioGraphNode {
    DeviceNode(ComponentArgs &&args, CreateNodeFunction &&create_node) : AudioGraphNode(std::move(args), std::move(create_node)) {
        Device = &GetDeviceMaNode()->Device;

        // During initial `DeviceMaNode` creation, we don't use the device props (`Name`/`Format`), since
        // the `DeviceNode` instance members are not fully initialized at the point of `AudioGraphNode` construction.
        // At this point, they are initialized, so we need to update the device appropriately.
        // (It's unfortunate that this technicality leads to potentially restarting the device after its initial creation.)
        // TODO there's a way to do this - make `DeviceMaNode` a component. See `FaustNode` for an example (and its `DspId` field).
        UpdateDeviceConfig();

        // The device may have a different configuration than what we requested. Update fields to reflect the actual device config.
        Name.Set_(GetConfigName(Device->GetInfo()));
        UpdateFormat();

        const Field::References listened_fields{Name, Format};
        for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    }

    virtual ~DeviceNode() = default;

    inline DeviceMaNode *GetDeviceMaNode() const { return static_cast<DeviceMaNode *>(Node.get()); }

    inline void UpdateDeviceConfig() {
        auto target_native_format = Format ? std::optional<DeviceDataFormat>(Format->ToDeviceDataFormat()) : std::nullopt;
        GetDeviceMaNode()->UpdateDeviceConfig({Graph->GetFormat(), std::move(target_native_format), Name});
        UpdateFormat();
    }

    inline static string GetDisplayName(const ma_device_info *info) { return !info ? "None" : (string(info->name) + (info->isDefault ? "*" : "")); }
    inline static string GetConfigName(const ma_device_info *info) { return info->isDefault ? "" : info->name; }

    string GetLabelDetailSuffix() const override { return Device->GetName(); }

    void OnSampleRateChanged() override {
        AudioGraphNode::OnSampleRateChanged();
        auto new_client_format = Graph->GetFormat();
        if (Device->GetClientFormat() != new_client_format) {
            UpdateDeviceConfig();
        }
    }

    void OnFieldChanged() override {
        AudioGraphNode::OnFieldChanged();
        if (Name.IsChanged() || Format.IsChanged(true)) {
            // If format-follow was just toggled on and the format values have never been set,
            // update `Format` to reflect the current native device format.
            // This does not require a device restart, since the format has not changed.
            if (Format.IsChanged() && Format && Format->SampleRate == 0u) UpdateFormat();
            else UpdateDeviceConfig();
        }
    }

    // If `Format` is set (if the native device format has been explicitly chosen by the user),
    // update its fields to reflect the current native device config.
    void UpdateFormat() {
        if (Format) Format->Set_(Device->GetNativeFormat());
    }

    // Mirrors `DeviceDataFormat`, as a component.
    struct DataFormat : Component {
        using Component::Component;

        static string GetFormatName(int format) { return DeviceDataFormat::GetFormatName(format); }

        void Set(DeviceDataFormat &&format) const {
            SampleFormat.Set(format.SampleFormat);
            Channels.Set(format.Channels);
            SampleRate.Set(format.SampleRate);
        }

        void Set_(DeviceDataFormat &&format) {
            SampleFormat.Set_(format.SampleFormat);
            Channels.Set_(format.Channels);
            SampleRate.Set_(format.SampleRate);
        }

        DeviceDataFormat ToDeviceDataFormat() const { return {SampleFormat, Channels, SampleRate}; }

        Prop(Enum, SampleFormat, GetFormatName);
        Prop(UInt, Channels);
        Prop(UInt, SampleRate);

    private:
        void Render() const override {
            const auto *device = static_cast<const DeviceNode *>(Parent->Parent);
            const auto device_data_format = ToDeviceDataFormat();
            if (BeginCombo(ImGuiLabel.c_str(), device_data_format.ToString().c_str())) {
                for (const auto &df : device->Device->GetNativeFormats()) {
                    const bool is_selected = device_data_format == df;
                    if (Selectable(df.ToString().c_str(), is_selected)) Action::AudioGraph::SetDeviceDataFormat{Id, df.SampleFormat, df.Channels, df.SampleRate}.q();
                    if (is_selected) SetItemDefaultFocus();
                }
                EndCombo();
            }
            HelpMarker();
        }
    };

    // When this is either empty or a device name that does not exist, the default device is used.
    Prop_(String, Name, "?An asterisk (*) indicates the default device.");

    // This is a native format target. If it is not set, the native format will follow the graph format.
    Prop(Optional<DataFormat>, Format);

    AudioDevice *Device;

private:
    void Render() const override {
        RenderDevice();
        Spacing();
        AudioGraphNode::Render();
    }

    void RenderDevice() const {
        if (!Device->IsStarted()) {
            TextUnformatted("Device is not started.");
            return;
        }

        SameLine(); // Assumes 'Delete' button is rendered by the graph immediately before this.
        if (Button("Rescan")) Device->ScanDevices();

        SetNextItemWidth(GetFontSize() * 14);
        const auto *device_info = Device->GetInfo();
        if (BeginCombo(Name.ImGuiLabel.c_str(), GetDisplayName(device_info).c_str())) {
            for (const auto *other_device_info : Device->GetAllInfos()) {
                const bool is_selected = device_info == other_device_info;
                if (Selectable(GetDisplayName(other_device_info).c_str(), is_selected)) {
                    Name.IssueSet(GetConfigName(other_device_info));
                }
                if (is_selected) SetItemDefaultFocus();
            }
            EndCombo();
        }
        Name.HelpMarker();

        SetNextItemWidth(GetFontSize() * 14);
        bool follow_graph_format = !Format;
        if (Checkbox("Follow graph format", &follow_graph_format)) Format.IssueToggle();
        SameLine();
        fg::HelpMarker(std::format(
            "When checked, this {0} device automatically follows the owning graph's sample rate and format. "
            "When the graph's sample rate changes, the device will be updated to use the native sample rate nearest to the graph's.\n\n"
            "When unchecked, this {0} device will be pinned to the selected native format, and will convert from the {1} format to the {2} format.\n"
            "See 'Device info' section for details on the device's current format conversion configuration.",
            to_string(Device->Type), Device->IsInput() ? "device" : "graph", Device->IsInput() ? "graph" : "device"
        ));

        if (Format) {
            Format->Draw();
        } else {
            TextUnformatted(Device->GetNativeFormat().ToString().c_str());
        }

        if (ImGui::TreeNode("Device info")) {
            Device->RenderInfo();
            TreePop();
        }
    }
};

struct InputDeviceMaNode : DeviceMaNode {
    InputDeviceMaNode(
        ma_node_graph *graph,
        AudioDevice::AudioCallback &&callback,
        AudioDevice::TargetConfig &&target_config,
        const void *client_user_data
    ) : DeviceMaNode(IO_In, std::move(callback), std::move(target_config), client_user_data) {
        // This min/max SR approach seems to work to get both upsampling and downsampling
        // (from low device SR to high client SR and vice versa), but it doesn't seem like the best approach.
        const auto [sr_min, sr_max] = std::minmax(Device.GetNativeSampleRate(), Device.GetClientFormat().SampleRate);
        const ma_device *device = Device.Get();
        ma_result result = ma_duplex_rb_init(ma_format_f32, device->capture.channels, sr_max, sr_min, device->capture.internalPeriodSizeInFrames, &device->pContext->allocationCallbacks, &DuplexRb);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize ring buffer: ", int(result)));

        auto node_config = ma_data_source_node_config_init(&DuplexRb);
        result = ma_data_source_node_init(graph, &node_config, nullptr, &SourceNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data source node: ", int(result)));

        Node = &SourceNode;
    }
    ~InputDeviceMaNode() {
        Device.Stop();
        ma_duplex_rb_uninit(&DuplexRb);
        ma_data_source_node_uninit(&SourceNode, nullptr);
    }

    ma_duplex_rb DuplexRb;
    ma_data_source_node SourceNode;
};

// A `ma_data_source_node` whose `ma_data_source` is a `ma_duplex_rb`.
// A source node that owns an input device and copies the device callback input buffer to a ring buffer.
struct InputDeviceNode : DeviceNode {
    InputDeviceNode(ComponentArgs &&args) : DeviceNode(std::move(args), [this] { return CreateNode(); }) {
        DuplexRb = &static_cast<InputDeviceMaNode *>(Node.get())->DuplexRb;
    }

    std::unique_ptr<MaNode> CreateNode() const {
        return std::make_unique<InputDeviceMaNode>(Graph->Get(), AudioInputCallback, AudioDevice::TargetConfig{Graph->GetFormat(), std::nullopt, ""}, this);
    }

    // Adapted from `ma_device__handle_duplex_callback_capture`.
    static void AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *user_data = reinterpret_cast<AudioDevice::UserData *>(device->pUserData);
        const auto *self = reinterpret_cast<const InputDeviceNode *>(user_data->User);
        if (self->Get() == nullptr) {
            ma_silence_pcm_frames(output, frame_count, device->capture.internalFormat, device->capture.channels);
            return;
        }
        ma_duplex_rb *duplex_rb = self->DuplexRb;

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

    ma_duplex_rb *DuplexRb;
};

struct OutputDeviceMaNode : DeviceMaNode {
    OutputDeviceMaNode(
        ma_node_graph *graph,
        bool is_primary,
        AudioDevice::AudioCallback &&callback,
        AudioDevice::TargetConfig &&target_config,
        const void *client_user_data
    ) : DeviceMaNode(IO_Out, std::move(callback), std::move(target_config), client_user_data) {
        const u32 device_channels = Device.GetClientFormat().Channels;
        if (!is_primary) Buffer = std::make_unique<BufferRef>(ma_format_f32, device_channels);
        auto node_config = ma_data_passthrough_node_config_init(device_channels, Buffer ? Buffer->Get() : nullptr);
        ma_result result = ma_data_passthrough_node_init(graph, &node_config, nullptr, &PassthroughNode);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the data passthrough node: ", int(result)));

        Node = &PassthroughNode;
    }
    ~OutputDeviceMaNode() {
        Device.Stop();
        ma_data_passthrough_node_uninit(&PassthroughNode, nullptr);
    }

    std::unique_ptr<BufferRef> Buffer;
    ma_data_passthrough_node PassthroughNode;
};

/*
`OutputDeviceNode` is a passthrough node that owns an output device.
It wraps around (custom) `ma_data_passthrough_node`, copying the input buffer in each callback to its internal `Buffer`.

Whenever there is at least one output device node, there is a single "primary" output device node.
The primary output device node passes the graph endpoint node's output buffer to the device callback output buffer.
Each remaining output device node populates its device callback output buffer with its buffer data (`ReadBufferData`).

The owning graph ensures the primary output device nodes is always connected directly to the graph endpoint node,
and each secondary node is connected to the graph endpoint node if it has at least one input node.
(This allows for checking `IsActive` state to silence the callback output instead of writing out an inactive buffer.)
*/
struct OutputDeviceNode : DeviceNode {
    inline static std::vector<OutputDeviceNode *> All; // The 'Primary' output device node is the first element.

    OutputDeviceNode(ComponentArgs &&args) : DeviceNode(std::move(args), [this] { return CreateNode(); }) {
        All.emplace_back(this);
    }
    ~OutputDeviceNode() {
        std::erase_if(All, [this](auto *node) { return node == this; });
    }

    std::unique_ptr<MaNode> CreateNode() const {
        return std::make_unique<OutputDeviceMaNode>(Graph->Get(), All.empty(), AudioOutputCallback, AudioDevice::TargetConfig{Graph->GetFormat(), std::nullopt, ""}, this);
    }

    // todo `SetPrimary(bool)`.
    // Switching from secondary to primary deletes the buffer and resets the node.
    // Switching from primary to secondary creates a buffer and resets the node.
    bool IsPrimary() const { return this == All.front(); }

    static void AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
        auto *user_data = reinterpret_cast<AudioDevice::UserData *>(device->pUserData);
        const auto *self = reinterpret_cast<const OutputDeviceNode *>(user_data->User);
        if (self->IsPrimary() && self->Graph) {
            ma_node_graph_read_pcm_frames(self->Graph->Get(), output, frame_count, nullptr);
        } else if (self->IsActive) {
            // After the primary output device node has pulled from the graph endpoint node,
            // This secondary output device node will have its input buses mixed and copied into its passthrough buffer.
            // Here, we forward these buffer frames to this device node's owned output audio device.
            self->ReadBufferData(output, frame_count);
        } else {
            ma_silence_pcm_frames(output, frame_count, device->playback.internalFormat, device->playback.channels);
        }

        (void)device;
        (void)input;
    }

    // Always connects directly/only to the graph endpoint node.
    bool AllowOutputConnectionChange() const override { return false; }

private:
    void ReadBufferData(void *output, u32 frame_count) const noexcept {
        auto &buffer = static_cast<OutputDeviceMaNode *>(Node.get())->Buffer;
        if (buffer) buffer->ReadData(output, frame_count);
    }

    inline void SetBufferData(const void *input, u32 frame_count) const {
        auto &buffer = static_cast<OutputDeviceMaNode *>(Node.get())->Buffer;
        if (buffer) buffer->SetData(input, frame_count);
    }
};

struct GraphMaNode : MaNode {
    GraphMaNode(u32 channels) {
        auto config = ma_node_graph_config_init(channels);
        ma_result result = ma_node_graph_init(&config, nullptr, &_Graph);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", int(result)));
        Node = ma_node_graph_get_endpoint(&_Graph);
    }
    ~GraphMaNode() {
        ma_node_graph_uninit(&_Graph, nullptr);
    }

    ma_node_graph _Graph;
};

AudioGraph::ChannelConverterNode::ChannelConverterNode(AudioGraph *graph, u32 from_channels, u32 to_channels)
    : Graph(graph) {
    Converter = std::make_unique<ma_channel_converter_node>();

    // todo log if no-op.
    auto config = ma_channel_converter_node_config_init(from_channels, to_channels);
    ma_result result = ma_channel_converter_node_init(Graph->Get(), &config, nullptr, Get());
    if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize channel converter node: {}", int(result))); }
}

AudioGraph::ChannelConverterNode::~ChannelConverterNode() {
    ma_channel_converter_node_uninit(Get(), nullptr);
}

ma_channel_converter_node *AudioGraph::ChannelConverterNode::Get() const { return Converter.get(); }

u32 AudioGraph::ChannelConverterNode::ChannelCount(IO io) const {
    return io == IO_In ? Converter->converter.channelsIn : Converter->converter.channelsOut;
}

AudioGraph::AudioGraph(ComponentArgs &&args) : AudioGraphNode(std::move(args), [this] { return CreateNode(); }) {
    IsActive = true; // The graph is always active, since it is always connected to itself.
    this->RegisterListener(this); // The graph listens to itself _as an audio graph node_.

    Nodes.EmplaceBack_(InputDeviceNodeTypeId);
    Nodes.back()->SetMuted(true); // External input is muted by default.
    Nodes.EmplaceBack_(OutputDeviceNodeTypeId);

    if (SampleRate == 0u) SampleRate.Set_(GetDefaultSampleRate());
    Nodes.EmplaceBack_(WaveformNodeTypeId);

    const Field::References listened_fields = {Nodes, Connections};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);

    // Set up default connections.
    // The device output -> graph endpoint node connection is handled in `UpdateConnections`.
    Connections.Connect(GetInputDeviceNodes().front()->Id, GetOutputDeviceNodes().front()->Id);
    UpdateConnections();
}

AudioGraph::~AudioGraph() {
    Nodes.Clear();
}

std::unique_ptr<MaNode> AudioGraph::CreateNode() const { return std::make_unique<GraphMaNode>(GetFormat().Channels); }

void AudioGraph::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();

    if (Nodes.IsChanged() || Connections.IsChanged()) {
        UpdateConnections();
    }
}

template<std::derived_from<AudioGraphNode> AudioGraphNodeSubType, typename... Args>
static std::unique_ptr<AudioGraphNodeSubType> CreateAudioGraphNode(AudioGraph *graph, Args &&...args) {
    auto node = std::make_unique<AudioGraphNodeSubType>(std::forward<Args>(args)...);
    node->RegisterListener(graph);
    if (graph->Focus()) graph->SelectedNodeId = node->Id; // Navigate to newly created node.
    return node;
}

dsp *AudioGraph::GetFaustDsp(ID id) const {
    if (DspById.contains(id)) return DspById.at(id);
    return nullptr;
}

static ID latest_dsp_id = 0; // TODO use `Vector::ChildInitializerFunction` to set the store during creation instead.

std::unique_ptr<AudioGraphNode> AudioGraph::CreateAudioGraphNode(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    ComponentArgs args{parent, path_segment, "", path_prefix_segment};
    auto *graph = static_cast<AudioGraph *>(parent->Parent);
    if (path_segment == InputDeviceNodeTypeId) return CreateAudioGraphNode<InputDeviceNode>(graph, std::move(args));
    if (path_segment == OutputDeviceNodeTypeId) return CreateAudioGraphNode<OutputDeviceNode>(graph, std::move(args));
    if (path_segment == WaveformNodeTypeId) return CreateAudioGraphNode<WaveformNode>(graph, std::move(args));
    if (path_segment == FaustNodeTypeId) {
        auto node = CreateAudioGraphNode<FaustNode>(graph, std::move(args), latest_dsp_id);
        latest_dsp_id = 0;
        return node;
    }

    return nullptr;
}

void AudioGraph::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::AudioGraph::CreateNode &a) {
            Nodes.EmplaceBack(a.node_type_id);
        },
        [this](const Action::AudioGraph::CreateFaustNode &a) {
            latest_dsp_id = a.dsp_id;
            Nodes.EmplaceBack(FaustNodeTypeId);
        },
        [this](const Action::AudioGraph::DeleteNode &a) {
            Nodes.EraseId(a.id);
            Connections.DisconnectOutput(a.id);
        },
        [](const Action::AudioGraph::SetDeviceDataFormat &a) {
            if (!Component::ById.contains(a.id)) throw std::runtime_error(std::format("No audio device data format with id {} exists.", a.id));

            auto *format = static_cast<const DeviceNode::DataFormat *>(Component::ById.at(a.id));
            format->Set({a.sample_format, a.channels, a.sample_rate});
        },
    );
}

ma_node_graph *AudioGraph::Get() { return &reinterpret_cast<GraphMaNode *>(Node.get())->_Graph; }
DeviceDataFormat AudioGraph::GetFormat() const { return {int(ma_format_f32), 2, SampleRate}; }

bool AudioGraph::IsNativeSampleRate(u32 sample_rate) const {
    for (const auto *device_node : GetInputDeviceNodes()) {
        if (!device_node->Device->IsNativeSampleRate(sample_rate)) return false;
    }
    for (const auto *device_node : GetOutputDeviceNodes()) {
        if (!device_node->Device->IsNativeSampleRate(sample_rate)) return false;
    }
    return true;
}

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

void AudioGraph::OnFaustDspChanged(ID id, dsp *) {
    for (auto &node : FindAllByPathSegment(FaustNodeTypeId)) {
        auto *faust_node = reinterpret_cast<FaustNode *>(node.get());
        if (faust_node->Id == id) faust_node->SetDsp(id);
    }
}
void AudioGraph::OnFaustDspAdded(ID id, dsp *dsp) {
    DspById[id] = dsp;
    OnFaustDspChanged(id, dsp);
}
void AudioGraph::OnFaustDspRemoved(ID id) {
    DspById.erase(id);
    OnFaustDspChanged(id, nullptr);
}

void AudioGraph::OnNodeConnectionsChanged(AudioGraphNode *) { UpdateConnections(); }

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
    ChannelConverterNodes.clear();

    // Always connect the primary device to the graph endpoint, and connect secondary devices with at least one input node.
    // This is the only section in the method that modifies `Connections`.
    for (auto *output_device_node : GetOutputDeviceNodes()) {
        if (output_device_node->IsPrimary() || Connections.SourceCount(output_device_node->Id) > 0) {
            Connections.Connect(output_device_node->Id, Id);
        } else {
            Connections.Disconnect(output_device_node->Id, Id);
        }
    }

    for (auto *node : Nodes) {
        node->SetActive(Connections.HasPath(node->Id, Id));
        node->DisconnectOutput();
    }

    // Set up internal node connections.
    for (auto *node : Nodes) {
        if (!node->IsActive) continue;

        if (node->InputBusCount() > 0) {
            if (auto *in_monitor = node->GetMonitorNode(IO_In)) {
                ma_node_attach_output_bus(in_monitor->Get(), 0, node->Get(), 0);
            }
            if (auto *in_gainer = node->GetGainerNode(IO_In)) {
                // Monitor after applying gain.
                if (auto *in_monitor = node->GetMonitorNode(IO_In)) ma_node_attach_output_bus(in_gainer->Get(), 0, in_monitor->Get(), 0);
                else ma_node_attach_output_bus(in_gainer->Get(), 0, node->Get(), 0);
            }
        }
        if (node->OutputBusCount() > 0) {
            if (auto *out_gainer = node->GetGainerNode(IO_Out)) {
                ma_node_attach_output_bus(node->Get(), 0, out_gainer->Get(), 0);
            }
            if (auto *out_monitor = node->GetMonitorNode(IO_Out)) {
                // Monitor after applying gain.
                if (auto *out_gainer = node->GetGainerNode(IO_Out)) ma_node_attach_output_bus(out_gainer->Get(), 0, out_monitor->Get(), 0);
                else ma_node_attach_output_bus(node->Get(), 0, out_monitor->Get(), 0);
            }
        }
    }

    // The graph does not keep itself in its `Nodes` list.
    // xxx This should be a `ranges::concat` instead of making a new vector, but I couldn't get it to work.
    auto destination_nodes = Nodes.View() | std::views::transform([](const auto &node) { return node.get(); }) | ranges::to<std::vector>();
    destination_nodes.emplace_back(this);

    for (auto *source_node : Nodes) {
        if (!source_node->IsActive || source_node->OutputBusCount() == 0) continue;

        const u32 destination_count = Connections.DestinationCount(source_node->Id);
        if (destination_count == 0) continue; // Should never hit this, since this would imply the node is inactive.

        if (destination_count == 1) {
            for (auto *destination_node : destination_nodes) {
                if (Connections.IsConnected(source_node->Id, destination_node->Id)) {
                    Connect(source_node->OutputNode(), 0, destination_node->InputNode(), 0);
                }
            }
        } else {
            // Connecting a single source to multiple destinations requires a splitter node.
            auto *splitter = source_node->CreateSplitter(destination_count);
            ma_node_attach_output_bus(source_node->OutputNode(), 0, splitter, 0);

            u32 splitter_bus = 0;
            for (auto *destination_node : destination_nodes) {
                if (Connections.IsConnected(source_node->Id, destination_node->Id)) {
                    Connect(splitter, splitter_bus++, destination_node->InputNode(), 0);
                }
            }
        }
    }
}

void AudioGraph::Connect(ma_node *source, u32 source_output_bus, ma_node *destination, u32 destination_input_bus) {
    const u32 out_channels = ma_node_get_output_channels(source, source_output_bus);
    const u32 in_channels = ma_node_get_input_channels(destination, destination_input_bus);
    if (out_channels != in_channels) {
        ChannelConverterNodes.emplace_back(std::make_unique<ChannelConverterNode>(this, out_channels, in_channels));
        ma_node_attach_output_bus(source, source_output_bus, ChannelConverterNodes.back()->Get(), 0);
        ma_node_attach_output_bus(ChannelConverterNodes.back()->Get(), 0, destination, destination_input_bus);
    } else {
        ma_node_attach_output_bus(source, source_output_bus, destination, destination_input_bus);
    }
}

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
        const string label = graph.Nodes.GetChildLabel(node);
        const float label_w = CalcTextSize(label.c_str()).x;
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

        const string label = graph.Nodes.GetChildLabel(out_node);
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, node_label_w.y}, std::format("{}:{}", label, to_string(IO_Out)).c_str());
        const string ellipsified_label = Ellipsify(label, node_label_w_no_padding.y);
        RenderConnectionsLabel(IO_Out, out_node, ellipsified_label, label_interaction_flags);
        out_count++;
    }

    // Input channel labels and mixer cells.
    u32 in_i = 0;
    for (const auto *in_node : graph.Nodes) {
        if (!in_node->CanConnectInput()) continue;

        SetCursorScreenPos(grid_top_left + ImVec2{-node_label_w.x, (cell_size + cell_gap) * in_i});
        const string label = graph.Nodes.GetChildLabel(in_node);
        const auto label_interaction_flags = fg::InvisibleButton({node_label_w.x, cell_size}, std::format("{}:{}", label, to_string(IO_In)).c_str());
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

void AudioGraph::RenderNodeCreateSelector() const {
    if (ImGui::TreeNode("Create")) {
        if (ImGui::TreeNode("Device")) {
            if (Button(InputDeviceNodeTypeId.c_str())) Action::AudioGraph::CreateNode{InputDeviceNodeTypeId}.q();
            SameLine();
            if (Button(OutputDeviceNodeTypeId.c_str())) Action::AudioGraph::CreateNode{OutputDeviceNodeTypeId}.q();
            TreePop();
        }
        if (ImGui::TreeNode("Generator")) {
            if (Button(WaveformNodeTypeId.c_str())) Action::AudioGraph::CreateNode{WaveformNodeTypeId}.q();
            TreePop();
        }
        if (!DspById.empty() && ImGui::TreeNode(FaustNodeTypeId.c_str())) {
            for (const auto &[id, _] : DspById) {
                if (Button(to_string(id).c_str())) Action::AudioGraph::CreateFaustNode{id}.q();
            }
            TreePop();
        }
        // todo miniaudio effects
        // if (ImGui::TreeNode("Effect")) {
        //     TreePop();
        // }
        TreePop();
    }
}

void AudioGraph::Render() const {
    SampleRate.Render(AudioDevice::PrioritizedSampleRates);
    AudioGraphNode::Render();

    if (SelectedNodeId != 0) {
        SetNextItemOpen(true);
        if (IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
    }

    if (ImGui::TreeNode(Nodes.ImGuiLabel.c_str())) {
        RenderNodeCreateSelector();

        for (const auto *node : Nodes) {
            if (SelectedNodeId != 0) {
                const bool is_node_selected = SelectedNodeId == node->Id;
                SetNextItemOpen(is_node_selected);
                if (is_node_selected && IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
            }
            const bool node_active = node->IsActive;
            // Similar to `ImGuiCol_TextDisabled`, but a bit lighter.
            if (!node_active) PushStyleColor(ImGuiCol_Text, {0.7f, 0.7f, 0.7f, 1.0f});
            const bool node_open = ImGui::TreeNode(node->ImGuiLabel.c_str(), "%s", Nodes.GetChildLabel(node, true).c_str());
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
