#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"

#include "Helper/String.h"
#include "Project/Audio/AudioDevice.h"

// Custom nodes.
#include "ma_gainer_node/ma_gainer_node.h"
#include "ma_monitor_node/fft_data.h"
#include "ma_monitor_node/ma_monitor_node.h"
#include "ma_monitor_node/window_functions.h"

AudioGraphNode::AudioGraphNode(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraphNodes *>(Parent)->Graph) {
    audio_device.SampleRate.RegisterChangeListener(this);

    const Field::References listened_fields = {On, Muted, Monitor, OutputLevel, SmoothOutputLevel, SmoothOutputLevelMs, WindowType};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
}
AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
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

void AudioGraphNode::OnFieldChanged() {
    if (On.IsChanged()) {
        if (On && Node == nullptr) Init();
        else if (!On && Node != nullptr) Uninit();
    }
    if (SmoothOutputLevel.IsChanged() || SmoothOutputLevelMs.IsChanged()) {
        UpdateGainer();
    }
    if (Monitor.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitor(io);
    }
    if (Muted.IsChanged() || OutputLevel.IsChanged()) {
        UpdateOutputLevel();
    }
    if (audio_device.SampleRate.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitorSampleRate(io);
    }
    if (WindowType.IsChanged()) {
        for (const IO io : IO_All) UpdateMonitorWindowFunction(io);
    }
    // Notify on field changes that can result in connection changes.
    if (On.IsChanged() || SmoothOutputLevel.IsChanged() || Monitor.IsChanged()) {
        for (auto *listener : Listeners) listener->OnNodeConnectionsChanged(this);
    }
}

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }

// The output node corresponds to the graph endpoint node.
// Technically, it has an output bus, but it's handled specially by miniaudio.
// Most importantly, it is not possible to attach the graph endpoint's node into any other node.
// Thus, we treat it strictly as a sink and hide the fact that it technically has an output bus, since it functionally does not.
Count AudioGraphNode::OutputBusCount() const { return IsOutput() ? 0 : ma_node_get_output_bus_count(Node); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Node, bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Node = DoInit();
    UpdateAll();
}

void AudioGraphNode::UpdateOutputLevel() {
    if (!On || OutputBusCount() != 1) return;

    const float output_level = Muted ? 0.f : float(OutputLevel);
    if (GainerNode) {
        ma_gainer_node_set_gain(GainerNode.get(), output_level);
        ma_node_set_output_bus_volume(Node, 0, 1);
    } else {
        ma_node_set_output_bus_volume(Node, 0, output_level);
    }
}

void AudioGraphNode::UpdateGainer() {
    if (OutputBusCount() == 1 && SmoothOutputLevel && !GainerNode) {
        GainerNode = std::unique_ptr<ma_gainer_node, GainerDeleter>(new ma_gainer_node());
        const Count smooth_time_frames = float(SmoothOutputLevelMs) * float(audio_device.SampleRate) / 1000.f;
        ma_gainer_node_config config = ma_gainer_node_config_init(OutputChannelCount(0), smooth_time_frames);
        const int result = ma_gainer_node_init(Graph->Get(), &config, nullptr, GainerNode.get());
        if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize gainer node: {}", result)); }
    } else if (!SmoothOutputLevel && GainerNode) {
        GainerNode.reset();
    }
}

void AudioGraphNode::UpdateMonitorSampleRate(IO io) {
    if (auto *monitor = GetMonitorNode(io)) {
        ma_monitor_set_sample_rate(monitor, ma_uint32(audio_device.SampleRate));
    }
}

void AudioGraphNode::UpdateMonitorWindowFunction(IO io) {
    if (auto *monitor = GetMonitorNode(io)) {
        auto window_func = GetWindowFunction(WindowType);
        if (window_func == nullptr) throw std::runtime_error(std::format("Failed to get window function for window type {}.", int(WindowType)));

        ma_monitor_apply_window_function(monitor, window_func);
    }
}

ma_monitor_node *AudioGraphNode::InitMonitorNode(IO io) {
    if (auto *monitor = GetMonitorNode(io)) return monitor;

    if (io == IO_In) InputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
    else OutputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
    return GetMonitorNode(io);
}

void AudioGraphNode::UninitMonitorNode(IO io) {
    if (io == IO_In) InputMonitorNode.reset();
    else OutputMonitorNode.reset();
}

void AudioGraphNode::UpdateMonitor(IO io) {
    auto *monitor = GetMonitorNode(io);
    if (!monitor && Monitor && BusCount(io) > 0) {
        monitor = InitMonitorNode(io);
        const auto *device = audio_device.Get();
        const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
        ma_monitor_node_config config = ma_monitor_node_config_init(ChannelCount(io, 0), device->playback.internalSampleRate, buffer_size);
        int result = ma_monitor_node_init(Graph->Get(), &config, nullptr, monitor);
        if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize {} monitor node: {}", to_string(io), result)); }

        UpdateMonitorWindowFunction(io);
    } else if (monitor && (!Monitor || InputBusCount() == 0)) {
        UninitMonitorNode(io);
    }
}

void AudioGraphNode::UpdateAll() {
    // Update nodes from earliest to latest in the signal path.
    UpdateMonitor(IO_In);
    UpdateGainer();
    UpdateMonitor(IO_Out);

    UpdateOutputLevel();
}

void AudioGraphNode::GainerDeleter::operator()(ma_gainer_node *gainer) {
    ma_gainer_node_uninit(gainer, nullptr);
}
void AudioGraphNode::SplitterDeleter::operator()(ma_splitter_node *splitter) {
    ma_splitter_node_uninit(splitter, nullptr);
}
void AudioGraphNode::MonitorDeleter::operator()(ma_monitor_node *monitor) {
    ma_monitor_node_uninit(monitor, nullptr);
}

void AudioGraphNode::Uninit() {
    if (Node == nullptr) return;

    // Disconnect from earliest to latest in the signal path.
    UninitMonitorNode(IO_In);
    GainerNode.reset();
    UninitMonitorNode(IO_Out);
    SplitterNodes.clear();
    DoUninit();
    ma_node_uninit(Node, nullptr);
    Node = nullptr;
}

void AudioGraphNode::ConnectTo(AudioGraphNode &to) {
    if (auto *to_input_monitor = to.GetMonitorNode(IO_In)) ma_node_attach_output_bus(to_input_monitor, 0, to.Node, 0);
    if (GainerNode) ma_node_attach_output_bus(Node, 0, GainerNode.get(), 0);
    if (auto *out_monitor = GetMonitorNode(IO_Out)) {
        if (GainerNode) ma_node_attach_output_bus(GainerNode.get(), 0, out_monitor, 0); // Monitor after applying gain.
        else ma_node_attach_output_bus(Node, 0, out_monitor, 0);
    }

    to.InputNodes.insert(this);
    OutputNodes.insert(&to);

    if (auto *currently_connected_to = ((ma_node_base *)OutputNode())->pOutputBuses[0].pInputNode) {
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

void AudioGraphNode::DisconnectAll() {
    ma_node_detach_output_bus(OutputNode(), 0);
    SplitterNodes.clear();

    // Clear cached pointers to input/output nodes.
    InputNodes.clear();
    OutputNodes.clear();
}

using namespace ImGui;

void AudioGraphNode::RenderMonitorWaveform(IO io) const {
    const auto *monitor = GetMonitorNode(io);
    if (monitor == nullptr) return;

    if (ImPlot::BeginPlot("Waveform", {-1, 160})) {
        const auto N = monitor->config.buffer_frames;
        ImPlot::SetupAxes("Frame", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, N, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
        if (IsActive) {
            for (Count channel_index = 0; channel_index < ChannelCount(io, 0); channel_index++) {
                const std::string channel_name = std::format("Channel {}", channel_index);
                ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                ImPlot::PlotLine(channel_name.c_str(), monitor->buffer, N);
                ImPlot::PopStyleVar();
            }
        }
        ImPlot::EndPlot();
    }
}

void AudioGraphNode::RenderMonitorMagnitudeSpectrum(IO io) const {
    const auto *monitor = GetMonitorNode(io);
    if (monitor == nullptr) return;

    if (ImPlot::BeginPlot("Magnitude spectrum", {-1, 160})) {
        static const float MIN_DB = -100;
        const fft_data *fft = monitor->fft;
        const Count N = monitor->config.buffer_frames;
        const Count N_2 = N / 2;
        const float fs = monitor->config.sample_rate;
        const float fs_n = fs / float(N);

        static std::vector<float> frequency(N_2);
        static std::vector<float> magnitude(N_2);
        frequency.resize(N_2);
        magnitude.resize(N_2);

        const auto *data = fft->data; // Complex values.
        for (Count i = 0; i < N_2; i++) {
            frequency[i] = fs_n * float(i);
            const float mag_linear = sqrtf(data[i][0] * data[i][0] + data[i][1] * data[i][1]) / float(N_2);
            magnitude[i] = ma_volume_linear_to_db(mag_linear);
        }

        ImPlot::SetupAxes("Frequency bin", "Magnitude (dB)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, fs / 2, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, MIN_DB, 0, ImGuiCond_Always);
        if (IsActive) {
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
    if (!IsOutput()) {
        On.Draw(); // Output node cannot be turned off, since it's the graph endpoint.
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

    if (On) {
        if (!InputNodes.empty() || !OutputNodes.empty()) {
            Text("Connections: %s%s%s", NodesToString(InputNodes, true).c_str(), Name.c_str(), NodesToString(OutputNodes, false).c_str());
        } else {
            TextUnformatted("No connections");
        }
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
            if (GetMonitorNode(io) == nullptr) continue;

            if (TreeNodeEx(to_string(io).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s buffer", StringHelper::Capitalize(to_string(io)).c_str())) {
                RenderMonitorWaveform(io);
                RenderMonitorMagnitudeSpectrum(io);
                TreePop();
            }
        }
    }
}
