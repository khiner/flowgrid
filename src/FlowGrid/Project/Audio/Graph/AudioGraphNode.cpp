#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"

#include "AudioGraphAction.h"
#include "Helper/String.h"
#include "Project/Audio/AudioDevice.h"

// Custom nodes.
#include "ma_gainer_node/ma_gainer_node.h"
#include "ma_monitor_node/fft_data.h"
#include "ma_monitor_node/ma_monitor_node.h"
#include "ma_monitor_node/window_functions.h"

using namespace ImGui;

AudioGraphNode::GainerNode::GainerNode(ComponentArgs &&args)
    : Component(std::move(args)), Node(static_cast<AudioGraphNode *>(Parent->Parent)), Gainer(std::make_unique<ma_gainer_node>()), SampleRate(Node->Graph->SampleRate) {
    SmoothTimeMs.RegisterChangeListener(this);
    const u32 smooth_time_frames = SmoothTimeMs * float(SampleRate) / 1000.f;
    auto config = ma_gainer_node_config_init(Node->OutputChannelCount(0), smooth_time_frames);
    ma_result result = ma_gainer_node_init(Node->Graph->Get(), &config, nullptr, Get());
    if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize gainer node: {}", int(result))); }
}

AudioGraphNode::GainerNode::~GainerNode() {
    ma_gainer_node_uninit(Get(), nullptr);
}

void AudioGraphNode::GainerNode::OnFieldChanged() {
    if (SmoothTimeMs.IsChanged()) UpdateSmoothTime();
}

ma_gainer_node *AudioGraphNode::GainerNode::Get() { return Gainer.get(); }

void AudioGraphNode::GainerNode::SetGain(float gain) { ma_gainer_node_set_gain(Get(), gain); }

void AudioGraphNode::GainerNode::UpdateSmoothTime() {
    const u32 smooth_time_frames = SmoothTimeMs * float(SampleRate) / 1000.f;
    ma_gainer_node_set_smooth_time_frames(Get(), smooth_time_frames);
}

void AudioGraphNode::GainerNode::SetSampleRate(u32 sample_rate) {
    if (SampleRate == sample_rate) return;
    SampleRate = sample_rate;
    UpdateSmoothTime();
}

struct AudioGraphNode::SplitterNode {
    SplitterNode(ma_node_graph *ma_graph, u32 channels) {
        auto config = ma_splitter_node_config_init(channels);
        ma_result result = ma_splitter_node_init(ma_graph, &config, nullptr, &Splitter);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", int(result)));
    }
    ~SplitterNode() {
        ma_splitter_node_uninit(&Splitter, nullptr);
    }

    inline ma_splitter_node *Get() noexcept { return &Splitter; }

private:
    ma_splitter_node Splitter;
};

struct AudioGraphNode::MonitorNode {
    MonitorNode(ma_node_graph *ma_graph, u32 channels, u32 buffer_frames) {
        auto config = ma_monitor_node_config_init(channels, buffer_frames);
        ma_result result = ma_monitor_node_init(ma_graph, &config, nullptr, &Monitor);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize monitor node: {}", int(result)));
    }
    ~MonitorNode() {
        ma_monitor_node_uninit(&Monitor, nullptr);
    }

    inline ma_monitor_node *Get() noexcept { return &Monitor; }

    void ApplyWindowFunction(WindowFunctionType window_function) {
        ma_monitor_apply_window_function(&Monitor, window_function);
    }

    void RenderWaveform(bool is_active) const {
        if (ImPlot::BeginPlot("Waveform", {-1, 160})) {
            const auto N = Monitor.config.buffer_frames;
            ImPlot::SetupAxes("Frame", "Value");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, N, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
            if (is_active) {
                for (u32 channel_index = 0; channel_index < Monitor.config.channels; channel_index++) {
                    const std::string channel_name = std::format("Channel {}", channel_index);
                    ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                    ImPlot::PlotLine(channel_name.c_str(), Monitor.buffer, N);
                    ImPlot::PopStyleVar();
                }
            }
            ImPlot::EndPlot();
        }
    }

    void RenderMagnitudeSpectrum(bool is_active, u32 sample_rate) const {
        if (ImPlot::BeginPlot("Magnitude spectrum", {-1, 160})) {
            static const float MIN_DB = -100;
            const fft_data *fft = Monitor.fft;
            const u32 N = Monitor.config.buffer_frames;
            const u32 N_2 = N / 2;
            const float fs = sample_rate;
            const float fs_n = fs / float(N);

            static std::vector<float> frequency(N_2);
            static std::vector<float> magnitude(N_2);
            frequency.resize(N_2);
            magnitude.resize(N_2);

            const auto *data = fft->data; // Complex values.
            for (u32 i = 0; i < N_2; i++) {
                frequency[i] = fs_n * float(i);
                const float mag_linear = sqrtf(data[i][0] * data[i][0] + data[i][1] * data[i][1]) / float(N_2);
                magnitude[i] = ma_volume_linear_to_db(mag_linear);
            }

            ImPlot::SetupAxes("Frequency bin", "Magnitude (dB)");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, fs / 2, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, MIN_DB, 0, ImGuiCond_Always);
            if (is_active) {
                ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                // ImPlot::PushStyleColor(ImPlotCol_Fill, {1.f, 0.f, 0.f, 1.f});
                // todo per-channel
                ImPlot::PlotShaded("1", frequency.data(), magnitude.data(), N_2, MIN_DB);
                // ImPlot::PopStyleColor();
                ImPlot::PopStyleVar();
            }
            ImPlot::EndPlot();
        }
    }

private:
    ma_monitor_node Monitor;
};

AudioGraphNode::AudioGraphNode(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Name == "Graph" ? this : Parent->Parent)) {
    const Field::References listened_fields = {Graph->SampleRate, Muted, OutputLevel, OutputGainer, Monitor, MonitorWindowLength, MonitorWindowType};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
}

AudioGraphNode::~AudioGraphNode() {
    Splitters.clear();
    InputMonitor.reset();
    OutputGainer.Reset();
    OutputMonitor.reset();
    if (Node != nullptr) {
        ma_node_uninit(Node, nullptr);
        Node = nullptr;
    }
    Listeners.clear();
    Field::UnregisterChangeListener(this);
}

ma_node *AudioGraphNode::InputNode() const {
    if (InputMonitor) return InputMonitor->Get();
    return Node;
}
ma_node *AudioGraphNode::OutputNode() const {
    if (OutputMonitor) return OutputMonitor->Get();
    if (OutputGainer) return OutputGainer->Get();
    return Node;
}

AudioGraphNode::MonitorNode *AudioGraphNode::GetMonitor(IO io) const {
    if (io == IO_In) return InputMonitor.get();
    if (io == IO_Out) return OutputMonitor.get();
    return nullptr;
}

WindowFunctionType GetWindowFunction(WindowType type) {
    switch (type) {
        case WindowType_Rectangular:
            return rectwin;
        case WindowType_Hann:
            return hann_periodic;
        case WindowType_Hamming:
            return hamming_periodic;
        case WindowType_Blackman:
            return blackman_periodic;
        case WindowType_BlackmanHarris:
            return blackmanharris_periodic;
        case WindowType_Nuttall:
            return nuttallwin_periodic;
        case WindowType_FlatTop:
            return flattopwin_periodic;
        case WindowType_Triangular:
            return triang;
        case WindowType_Bartlett:
            return bartlett;
        case WindowType_BartlettHann:
            return barthannwin;
        case WindowType_Bohman:
            return bohmanwin;
        case WindowType_Parzen:
            return parzenwin;
        default:
            return nullptr;
    }
}

void AudioGraphNode::OnSampleRateChanged() {
    if (OutputGainer) OutputGainer->SetSampleRate(Graph->SampleRate);
}

void AudioGraphNode::OnFieldChanged() {
    if (Graph->SampleRate.IsChanged()) {
        OnSampleRateChanged();
    }
    if (OutputGainer.IsChanged()) {
        UpdateOutputLevel();
    }
    if (Monitor.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitor(io);
    }
    if (Muted.IsChanged() || OutputLevel.IsChanged()) {
        UpdateOutputLevel();
    }
    if (MonitorWindowLength.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitorWindowLength(io);
    }
    if (MonitorWindowType.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitorWindowFunction(io);
    }
    // Notify on field changes that can result in connection changes.
    if (OutputGainer.IsChanged() || Monitor.IsChanged() || MonitorWindowLength.IsChanged()) {
        for (auto *listener : Listeners) listener->OnNodeConnectionsChanged(this);
    }
}

u32 AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }

// Technically, the graph endpoint node has an output bus, but it's handled specially by miniaudio.
// Most importantly, it is not possible to attach the graph endpoint's node into any other node.
// Thus, we treat it strictly as a sink and hide the fact that it technically has an output bus, since it functionally does not.
u32 AudioGraphNode::OutputBusCount() const { return IsGraphEndpoint() ? 0 : ma_node_get_output_bus_count(Node); }

u32 AudioGraphNode::InputChannelCount(u32 bus) const { return ma_node_get_input_channels(Node, bus); }
u32 AudioGraphNode::OutputChannelCount(u32 bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::UpdateOutputLevel() {
    if (OutputBusCount() == 0) return;

    const float output_level = Muted ? 0.f : float(OutputLevel);
    if (OutputGainer) {
        OutputGainer->SetGain(output_level);
        ma_node_set_output_bus_volume(Node, 0, 1);
    } else {
        ma_node_set_output_bus_volume(Node, 0, output_level);
    }
}

void AudioGraphNode::CreateMonitor(IO io) {
    auto monitor = std::make_unique<MonitorNode>(Graph->Get(), ChannelCount(io, 0), MonitorWindowLength);
    if (io == IO_In) InputMonitor = std::move(monitor);
    else OutputMonitor = std::move(monitor);

    UpdateMonitorWindowFunction(io);
}

void AudioGraphNode::UpdateMonitorWindowLength(IO io) {
    if (GetMonitor(io)) CreateMonitor(io); // Recreate the monitor node to update the buffer size.
}

void AudioGraphNode::UpdateMonitorWindowFunction(IO io) {
    if (auto *monitor = GetMonitor(io)) {
        auto window_function = GetWindowFunction(MonitorWindowType);
        if (window_function == nullptr) throw std::runtime_error(std::format("Failed to get window function for window type {}.", int(MonitorWindowType)));

        monitor->ApplyWindowFunction(std::move(window_function));
    }
}

void AudioGraphNode::UpdateMonitor(IO io) {
    auto *monitor = GetMonitor(io);
    auto bus_count = BusCount(io);
    if (!monitor && Monitor && bus_count > 0) {
        CreateMonitor(io);
    } else if (monitor && (!Monitor || bus_count == 0)) {
        if (io == IO_In) InputMonitor.reset();
        else OutputMonitor.reset();
    }
}

void AudioGraphNode::UpdateAll() {
    // Update nodes from earliest to latest in the signal path.
    UpdateMonitor(IO_In);
    OutputGainer.Refresh();
    UpdateMonitor(IO_Out);

    UpdateOutputLevel();
}

void AudioGraphNode::ConnectTo(AudioGraphNode &to) {
    if (auto *to_input_monitor = to.GetMonitor(IO_In)) ma_node_attach_output_bus(to_input_monitor->Get(), 0, to.Node, 0);
    if (OutputGainer) ma_node_attach_output_bus(Node, 0, OutputGainer->Get(), 0);
    if (OutputMonitor) {
        if (OutputGainer) ma_node_attach_output_bus(OutputGainer->Get(), 0, OutputMonitor->Get(), 0); // Monitor after applying gain.
        else ma_node_attach_output_bus(Node, 0, OutputMonitor->Get(), 0);
    }

    auto *output_node = OutputNode();
    if (auto *currently_connected_to = ((ma_node_base *)output_node)->pOutputBuses[0].pInputNode) {
        // Connecting a single source to multiple destinations requires a splitter node.
        // We chain splitters together to support any number of destinations.
        Splitters.emplace_back(std::make_unique<SplitterNode>(Graph->Get(), OutputChannelCount(0)));
        auto *splitter = Splitters.back()->Get();
        ma_node_attach_output_bus(splitter, 0, currently_connected_to, 0);
        ma_node_attach_output_bus(splitter, 1, to.InputNode(), 0);
        ma_node_attach_output_bus(OutputNode(), 0, splitter, 0);
    } else {
        ma_node_attach_output_bus(OutputNode(), 0, to.InputNode(), 0);
    }
}

void AudioGraphNode::DisconnectAll() {
    ma_node_detach_output_bus(OutputNode(), 0);
    Splitters.clear();
}

std::string AudioGraphNode::GetWindowLengthName(u32 window_length_frames) const {
    return std::format("{} ({:.2f} ms)", window_length_frames, float(window_length_frames * 1000) / float(Graph->SampleRate));
}

std::string NodesToString(const std::unordered_set<AudioGraphNode *> &nodes, bool is_input) {
    if (nodes.empty()) return "";

    std::string str;
    for (const auto *node : nodes) {
        str += node->Name;
        str += ", ";
    }

    if (!nodes.empty()) str.resize(str.size() - 2);
    if (nodes.size() > 1) str = std::format("({})", str);

    return is_input ? std::format("{} -> ", str) : std::format(" -> {}", str);
}

void AudioGraphNode::Render() const {
    if (!IsGraphEndpoint()) {
        if (Button("X")) {
            Action::AudioGraph::DeleteNode{Id}.q();
        }
        SameLine();
    }

    if (IsActive) {
        PushStyleColor(ImGuiCol_Text, {0.0f, 1.0f, 0.0f, 1.0f});
        TextUnformatted("Active");
    } else {
        PushStyleColor(ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f});
        TextUnformatted("Inactive");
    }
    PopStyleColor();

    if (TreeNode("Connections")) {
        auto source_nodes = Graph->GetSourceNodes(this);
        auto destination_nodes = Graph->GetDestinationNodes(this);
        if (!source_nodes.empty() || !destination_nodes.empty()) {
            Text("%s%s%s", NodesToString(source_nodes, true).c_str(), Name.c_str(), NodesToString(destination_nodes, false).c_str());
        } else {
            TextUnformatted("No connections");
        }
        TreePop();
    }

    Spacing();
    Muted.Draw();
    OutputLevel.Draw();
    if (OutputBusCount() > 0) OutputGainer.Draw();

    Spacing();
    Monitor.Draw();
    if (Monitor) {
        static const std::vector<u32> WindowLengthOptions = {256, 512, 1024, 2048, 4096, 8192, 16384};
        SetNextItemWidth(GetFontSize() * 9);
        MonitorWindowLength.Render(WindowLengthOptions);
        SetNextItemWidth(GetFontSize() * 9);
        MonitorWindowType.Draw();
        for (const IO io : IO_All) {
            if (GetMonitor(io) == nullptr) continue;

            if (TreeNodeEx(to_string(io).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s buffer", StringHelper::Capitalize(to_string(io)).c_str())) {
                if (const auto *monitor = GetMonitor(io)) {
                    monitor->RenderWaveform(IsActive);
                    monitor->RenderMagnitudeSpectrum(IsActive, Graph->SampleRate);
                }
                TreePop();
            }
        }
    }
}
