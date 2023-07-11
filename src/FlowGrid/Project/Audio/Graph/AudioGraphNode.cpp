#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "miniaudio.h"

#include "Helper/String.h"
#include "Project/Audio/AudioDevice.h"

AudioGraphNode::AudioGraphNode(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {
    Volume.RegisterChangeListener(this);
    Muted.RegisterChangeListener(this);
}
AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
}

void AudioGraphNode::OnFieldChanged() {
    if (Muted.IsChanged() || Volume.IsChanged()) UpdateVolume();
}

void AudioGraphNode::Set(ma_node *node) { Node = node; }

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }

// Output node (graph endpoint) technically has an output bus, but it doesn't work like other nodes and we treat it strictly as a sink.
Count AudioGraphNode::OutputBusCount() const { return IsOutput() ? 0 : ma_node_get_output_bus_count(Node); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Node, bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Set(DoInit());
    UpdateMonitors();
    UpdateVolume();
}

void AudioGraphNode::UpdateVolume() {
    if (On) ma_node_set_output_bus_volume(Node, 0, Muted ? 0.f : float(Volume));
}

void AudioGraphNode::UpdateMonitors() {
    if (InputBusCount() > 0) {
        if (Monitor && !InputMonitorNode) {
            InputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
            const auto *device = audio_device.Get();
            const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
            ma_monitor_node_config config = ma_monitor_node_config_init(InputChannelCount(0), device->playback.internalSampleRate, buffer_size);
            int result = ma_monitor_node_init(Graph->Get(), &config, nullptr, InputMonitorNode.get());
            if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize input monitor node: {}", result)); }
        } else if (!Monitor && InputMonitorNode) {
            InputMonitorNode.reset();
        }
    }

    if (OutputBusCount() > 0) {
        if (Monitor && !OutputMonitorNode) {
            OutputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
            const auto *device = audio_device.Get();
            const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
            ma_monitor_node_config config = ma_monitor_node_config_init(OutputChannelCount(0), device->playback.internalSampleRate, buffer_size);
            int result = ma_monitor_node_init(Graph->Get(), &config, nullptr, OutputMonitorNode.get());
            if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize output monitor node: {}", result)); }
        } else if (!Monitor && OutputMonitorNode) {
            OutputMonitorNode.reset();
        }
    }
}

void AudioGraphNode::Update() {
    const bool is_initialized = Node != nullptr;
    if (On && !is_initialized) Init();
    else if (!On && is_initialized) Uninit();

    UpdateMonitors();
    UpdateVolume();
}

void AudioGraphNode::SplitterDeleter::operator()(ma_splitter_node *splitter) {
    ma_splitter_node_uninit(splitter, nullptr);
}
void AudioGraphNode::MonitorDeleter::operator()(ma_monitor_node *monitor) {
    ma_monitor_node_uninit(monitor, nullptr);
}

void AudioGraphNode::Uninit() {
    if (Node == nullptr) return;

    SplitterNodes.clear();
    OutputMonitorNode.reset();
    InputMonitorNode.reset();
    DoUninit();
    ma_node_uninit(Node, nullptr);
    Set(nullptr);
}

void AudioGraphNode::ConnectTo(const AudioGraphNode &to) {
    if (to.InputMonitorNode) ma_node_attach_output_bus(to.InputMonitorNode.get(), 0, to.Node, 0);
    if (OutputMonitorNode) ma_node_attach_output_bus(Node, 0, OutputMonitorNode.get(), 0);

    auto *currently_connected_to = ((ma_node_base *)OutputNode())->pOutputBuses[0].pInputNode;
    if (currently_connected_to != nullptr) {
        // Connecting a single source to multiple destinations requires a splitter node.
        // We chain splitters together to support any number of destinations.
        // Note: `new` is necessary here because we use a custom deleter.
        SplitterNodes.emplace_back(new ma_splitter_node());
        ma_splitter_node *splitter = SplitterNodes.back().get();
        ma_splitter_node_config splitter_config = ma_splitter_node_config_init(OutputChannelCount(0));
        int result = ma_splitter_node_init(Graph->Get(), &splitter_config, nullptr, splitter);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", result));

        ma_node_attach_output_bus(splitter, 0, currently_connected_to, 0);
        ma_node_attach_output_bus(splitter, 1, to.InputNode(), 0);
        ma_node_attach_output_bus(OutputNode(), 0, splitter, 0);
    } else {
        ma_node_attach_output_bus(OutputNode(), 0, to.InputNode(), 0);
    }
}

void AudioGraphNode::DisconnectOutputs() {
    ma_node_detach_output_bus(OutputNode(), 0);
    SplitterNodes.clear();
}

using namespace ImGui;

void AudioGraphNode::RenderMonitor(IO io) const {
    const auto *monitor_node = GetMonitorNode(io);
    if (monitor_node == nullptr) return;

    if (ImPlot::BeginPlot(StringHelper::Capitalize(to_string(io)).c_str(), {-1, 160})) {
        const Count frame_count = monitor_node->bufferSizeInFrames;

        ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
        ImPlot::SetupAxes("Buffer frame", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
        for (Count channel_index = 0; channel_index < ChannelCount(io, 0); channel_index++) {
            const std::string channel_name = std::format("Channel {}", channel_index);
            ImPlot::PlotLine(channel_name.c_str(), monitor_node->pBuffer, frame_count);
        }
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}

void AudioGraphNode::Render() const {
    if (!IsOutput()) On.Draw(); // Output node cannot be turned off, since it's the graph endpoint.

    if (IsActive) {
        PushStyleColor(ImGuiCol_Text, {0.0f, 1.0f, 0.0f, 1.0f});
        TextUnformatted("Active");
    } else {
        PushStyleColor(ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f});
        TextUnformatted("Inactive");
    }
    PopStyleColor();

    Muted.Draw();
    SameLine();
    Volume.Draw();
    Monitor.Draw();
    if (Monitor) {
        for (IO io : IO_All) RenderMonitor(io);
    }
}
