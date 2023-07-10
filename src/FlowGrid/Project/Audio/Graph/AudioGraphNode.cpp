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
    MonitorOutput.RegisterChangeListener(this);
}
AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
}

void AudioGraphNode::OnFieldChanged() {
    if (Muted.IsChanged() || Volume.IsChanged()) UpdateVolume();
    if (MonitorOutput.IsChanged()) UpdateMonitors();
}

void AudioGraphNode::Set(ma_node *node) {
    Node = node;
}

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }
Count AudioGraphNode::OutputBusCount() const { return ma_node_get_output_bus_count(Node); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Node, bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Set(DoInit());
    UpdateVolume();
    UpdateMonitors();
}

void AudioGraphNode::UpdateVolume() {
    if (On) ma_node_set_output_bus_volume(Node, 0, Muted ? 0.f : float(Volume));
}

void AudioGraphNode::UpdateMonitors() {
    if (MonitorOutput && !OutputMonitorNode) {
        OutputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
        const auto *device = audio_device.Get();
        const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
        ma_monitor_node_config config = ma_monitor_node_config_init(device->playback.channels, device->playback.internalSampleRate, buffer_size);
        int result = ma_monitor_node_init(Graph->Get(), &config, NULL, OutputMonitorNode.get());
        if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize output monitor node: {}", result)); }

        // If this node is connected to another node, insert the monitor node between them.
        auto *connected_to_node = ((ma_node_base *)Node)->pOutputBuses[0].pInputNode;
        if (connected_to_node != nullptr) {
            ma_node_attach_output_bus(OutputMonitorNode.get(), 0, connected_to_node, 0);
            ma_node_attach_output_bus(Node, 0, OutputMonitorNode.get(), 0);
        }
    } else if (!MonitorOutput && OutputMonitorNode) {
        // If the monitor node is connected to another node, connect this node to that node.
        auto *connected_to_node = ((ma_node_base *)OutputMonitorNode.get())->pOutputBuses[0].pInputNode;
        if (connected_to_node != nullptr) {
            ma_node_attach_output_bus(Node, 0, connected_to_node, 0);
        }
        OutputMonitorNode.reset();
    }
}

void AudioGraphNode::Update() {
    const bool is_initialized = Node != nullptr;
    if (On && !is_initialized) Init();
    else if (!On && is_initialized) Uninit();

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
    DoUninit();
    ma_node_uninit(Node, nullptr);
    Set(nullptr);
}

void AudioGraphNode::ConnectTo(const AudioGraphNode &to) {
    if (OutputMonitorNode) {
        ma_node_attach_output_bus(Node, 0, OutputMonitorNode.get(), 0);
        ma_node_attach_output_bus(OutputMonitorNode.get(), 0, to.Node, 0);
    } else {
        ma_node_attach_output_bus(Node, 0, to.Node, 0);
    }
}

void AudioGraphNode::DisconnectOutputs() {
    ma_node_detach_output_bus(Node, 0);
    SplitterNodes.clear();
}

using namespace ImGui;

void AudioGraphNode::Render() const {
    On.Draw();
    Muted.Draw();
    SameLine();
    Volume.Draw();
    if (TreeNode("Plots")) {
        if (!MonitorOutput) MonitorOutput.Toggle();
        for (IO io : IO_All) {
            const bool is_in = io == IO_In;
            if (is_in) continue; // xxx temporary
            if (!OutputMonitorNode) continue;

            if (TreeNodeEx(StringHelper::Capitalize(to_string(io)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImPlot::BeginPlot("Waveform", {-1, 160})) {
                    ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                    ImPlot::SetupAxes("Buffer frame", "Value");
                    const Count frame_count = OutputMonitorNode->bufferSizeInFrames;
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
                    for (Count channel_index = 0; channel_index < 1; channel_index++) {
                        const float *buffer = OutputMonitorNode->pBuffer;
                        const std::string channel_name = std::format("Channel {}", channel_index);
                        ImPlot::PlotLine(channel_name.c_str(), buffer, frame_count);
                    }
                    ImPlot::PopStyleVar();
                    ImPlot::EndPlot();
                }
                TreePop();
            }
        }
        TreePop();
    } else if (MonitorOutput) {
        MonitorOutput.Toggle();
    }
}
