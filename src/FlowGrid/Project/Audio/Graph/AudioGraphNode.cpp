#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"

#include "Helper/String.h"
#include "Project/Audio/AudioDevice.h"
#include "AudioGraphAction.h"

// Custom nodes.
#include "ma_gainer_node/ma_gainer_node.h"
#include "ma_monitor_node/fft_data.h"
#include "ma_monitor_node/ma_monitor_node.h"
#include "ma_monitor_node/window_functions.h"

using namespace ImGui;

struct AudioGraphNode::GainerNode {
    GainerNode(ma_node_graph *ma_graph, u32 channels, u32 sample_rate, float smooth_time_ms)
        : SmoothTimeMs(smooth_time_ms) {
        const u32 smooth_time_frames = SmoothTimeMs * float(sample_rate) / 1000.f;
        ma_gainer_node_config config = ma_gainer_node_config_init(channels, smooth_time_frames);
        const int result = ma_gainer_node_init(ma_graph, &config, nullptr, &Gainer);
        if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize gainer node: {}", result)); }
    }
    ~GainerNode() {
        ma_gainer_node_uninit(&Gainer, nullptr);
    }

    inline ma_gainer_node *Get() noexcept { return &Gainer; }

    void SetGain(float gain) {
        ma_gainer_node_set_gain(&Gainer, gain);
    }

    void SetSampleRate(u32 sample_rate) {
        const u32 smooth_time_frames = SmoothTimeMs * float(sample_rate) / 1000.f;
        ma_gainer_node_set_smooth_time_frames(&Gainer, smooth_time_frames);
    }

private:
    ma_gainer_node Gainer;
    float SmoothTimeMs;
};

struct AudioGraphNode::SplitterNode {
    SplitterNode(ma_node_graph *ma_graph, u32 channels) {
        ma_splitter_node_config config = ma_splitter_node_config_init(channels);
        int result = ma_splitter_node_init(ma_graph, &config, nullptr, &Splitter);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", result));
    }
    ~SplitterNode() {
        ma_splitter_node_uninit(&Splitter, nullptr);
    }

    inline ma_splitter_node *Get() noexcept { return &Splitter; }

private:
    ma_splitter_node Splitter;
};

struct AudioGraphNode::MonitorNode {
    MonitorNode(ma_node_graph *ma_graph, u32 channels, u32 sample_rate, u32 buffer_frames) {
        ma_monitor_node_config config = ma_monitor_node_config_init(channels, sample_rate, buffer_frames);
        int result = ma_monitor_node_init(ma_graph, &config, nullptr, &Monitor);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize monitor node: {}", result));
    }
    ~MonitorNode() {
        ma_monitor_node_uninit(&Monitor, nullptr);
    }

    inline ma_monitor_node *Get() noexcept { return &Monitor; }

    void SetSampleRate(u32 sample_rate) {
        ma_monitor_set_sample_rate(&Monitor, sample_rate);
    }

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

    void RenderMagnitudeSpectrum(bool is_active) const {
        if (ImPlot::BeginPlot("Magnitude spectrum", {-1, 160})) {
            static const float MIN_DB = -100;
            const fft_data *fft = Monitor.fft;
            const u32 N = Monitor.config.buffer_frames;
            const u32 N_2 = N / 2;
            const float fs = Monitor.config.sample_rate;
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

AudioGraphNode::AudioGraphNode(Component *parent, string_view path_segment)
    : Component(ComponentArgs{parent, path_segment}), Graph(static_cast<const AudioGraph *>(Parent->Parent)) {
    const Field::References listened_fields = {Muted, Monitor, OutputLevel, SmoothOutputLevel, SmoothOutputLevelMs, WindowType};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
}

AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
    Uninit();
}

u32 AudioGraphNode::GetDeviceSampleRate() const { return Graph->GetDeviceSampleRate(); }
u32 AudioGraphNode::GetDeviceBufferSize() const { return Graph->GetDeviceBufferSize(); }

ma_node *AudioGraphNode::InputNode() const {
    if (InputMonitor) return InputMonitor->Get();
    return Node;
}
ma_node *AudioGraphNode::OutputNode() const {
    if (OutputMonitor) return OutputMonitor->Get();
    if (Gainer) return Gainer->Get();
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

void AudioGraphNode::OnDeviceSampleRateChanged() {
    const u32 sample_rate = GetDeviceSampleRate();
    for (const IO io : IO_All) {
        if (auto *monitor = GetMonitor(io)) {
            monitor->SetSampleRate(sample_rate);
        }
    }
    if (Gainer) Gainer->SetSampleRate(sample_rate);
}

void AudioGraphNode::OnFieldChanged() {
    if (SmoothOutputLevel.IsChanged() || SmoothOutputLevelMs.IsChanged()) {
        UpdateGainer();
    }
    if (Monitor.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitor(io);
    }
    if (Muted.IsChanged() || OutputLevel.IsChanged()) {
        UpdateOutputLevel();
    }
    if (WindowType.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitorWindowFunction(io);
    }
    // Notify on field changes that can result in connection changes.
    if (SmoothOutputLevel.IsChanged() || Monitor.IsChanged()) {
        for (auto *listener : Listeners) listener->OnNodeConnectionsChanged(this);
    }
}

u32 AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }
u32 AudioGraphNode::OutputBusCount() const { return ma_node_get_output_bus_count(Node); }
u32 AudioGraphNode::InputChannelCount(u32 bus) const { return ma_node_get_input_channels(Node, bus); }
u32 AudioGraphNode::OutputChannelCount(u32 bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Node = DoInit(Graph->Get());
    UpdateAll();
}

void AudioGraphNode::UpdateOutputLevel() {
    if (OutputBusCount() == 0) return;

    const float output_level = Muted ? 0.f : float(OutputLevel);
    if (Gainer) {
        Gainer->SetGain(output_level);
        ma_node_set_output_bus_volume(Node, 0, 1);
    } else {
        ma_node_set_output_bus_volume(Node, 0, output_level);
    }
}

void AudioGraphNode::UpdateGainer() {
    if (OutputBusCount() > 0 && SmoothOutputLevel && !Gainer) {
        Gainer = std::make_unique<GainerNode>(Graph->Get(), OutputChannelCount(0), GetDeviceSampleRate(), SmoothOutputLevelMs);
        UpdateOutputLevel();
    } else if (!SmoothOutputLevel && Gainer) {
        Gainer.reset();
        UpdateOutputLevel();
    }
}

void AudioGraphNode::UpdateMonitorWindowFunction(IO io) {
    if (auto *monitor = GetMonitor(io)) {
        auto window_function = GetWindowFunction(WindowType);
        if (window_function == nullptr) throw std::runtime_error(std::format("Failed to get window function for window type {}.", int(WindowType)));

        monitor->ApplyWindowFunction(window_function);
    }
}

void AudioGraphNode::UpdateMonitor(IO io) {
    auto *monitor = GetMonitor(io);
    if (!monitor && Monitor && BusCount(io) > 0) {
        auto monitor = std::make_unique<MonitorNode>(Graph->Get(), ChannelCount(io, 0), GetDeviceSampleRate(), GetDeviceBufferSize());
        if (io == IO_In) InputMonitor = std::move(monitor);
        else OutputMonitor = std::move(monitor);

        UpdateMonitorWindowFunction(io);
    } else if (monitor && (!Monitor || InputBusCount() == 0)) {
        if (io == IO_In) InputMonitor.reset();
        else OutputMonitor.reset();
    }
}

void AudioGraphNode::UpdateAll() {
    // Update nodes from earliest to latest in the signal path.
    UpdateMonitor(IO_In);
    UpdateGainer();
    UpdateMonitor(IO_Out);

    UpdateOutputLevel();
}

void AudioGraphNode::Uninit() {
    if (Node == nullptr) return;

    // Disconnect from earliest to latest in the signal path.
    InputMonitor.reset();
    Gainer.reset();
    OutputMonitor.reset();
    Splitters.clear();
    DoUninit();
    ma_node_uninit(Node, nullptr);
    Node = nullptr;
}

void AudioGraphNode::ConnectTo(AudioGraphNode &to) {
    if (auto *to_input_monitor = to.GetMonitor(IO_In)) ma_node_attach_output_bus(to_input_monitor->Get(), 0, to.Node, 0);
    if (Gainer) ma_node_attach_output_bus(Node, 0, Gainer->Get(), 0);
    if (OutputMonitor) {
        if (Gainer) ma_node_attach_output_bus(Gainer->Get(), 0, OutputMonitor->Get(), 0); // Monitor after applying gain.
        else ma_node_attach_output_bus(Node, 0, OutputMonitor->Get(), 0);
    }

    to.InputNodes.insert(this);
    OutputNodes.insert(&to);

    if (auto *currently_connected_to = ((ma_node_base *)OutputNode())->pOutputBuses[0].pInputNode) {
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

    // Clear cached pointers to input/output nodes.
    InputNodes.clear();
    OutputNodes.clear();
}

std::string NodesToString(const std::unordered_set<const AudioGraphNode *> &nodes, bool is_input) {
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
    if (AllowDelete()) {
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

    if (!InputNodes.empty() || !OutputNodes.empty()) {
        Text("Connections: %s%s%s", NodesToString(InputNodes, true).c_str(), Name.c_str(), NodesToString(OutputNodes, false).c_str());
    } else {
        TextUnformatted("No connections");
    }

    Spacing();
    Muted.Draw();
    OutputLevel.Draw();
    SmoothOutputLevel.Draw();

    Spacing();
    Monitor.Draw();
    if (Monitor) {
        SameLine();
        SetNextItemWidth(GetFontSize() * 9);
        WindowType.Draw();
        for (const IO io : IO_All) {
            if (GetMonitor(io) == nullptr) continue;

            if (TreeNodeEx(to_string(io).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s buffer", StringHelper::Capitalize(to_string(io)).c_str())) {
                if (const auto *monitor = GetMonitor(io)) {
                    monitor->RenderWaveform(IsActive);
                    monitor->RenderMagnitudeSpectrum(IsActive);
                }
                TreePop();
            }
        }
    }
}
