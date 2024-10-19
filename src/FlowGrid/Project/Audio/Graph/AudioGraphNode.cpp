#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"

#include "Core/UI/HelpMarker.h"
#include "Helper/String.h"

// Custom nodes.
#include "ma_gainer_node/ma_gainer_node.h"
#include "ma_monitor_node/fft_data.h"
#include "ma_monitor_node/ma_monitor_node.h"
#include "ma_monitor_node/window_functions.h"
#include "ma_panner_node/ma_panner_node.h"

using namespace ImGui;

AudioGraphNode::GainerNode::GainerNode(ComponentArgs &&args)
    : Component(std::move(args)), ParentNode(static_cast<AudioGraphNode *>(Parent->Parent)), SampleRate(ParentNode->Graph->SampleRate) {
    Component::References listening_to = {Muted, Level, Smooth};
    for (const auto &component : listening_to) component.get().RegisterChangeListener(this);

    Gainer = std::make_unique<ma_gainer_node>();
    Init();
}

AudioGraphNode::GainerNode::~GainerNode() {
    Uninit();
}

void AudioGraphNode::GainerNode::Init() {
    const u32 smooth_time_frames = Smooth ? (float(SmoothTimeMs) * float(SampleRate) / 1000.f) : 0;
    auto config = ma_gainer_node_config_init(ParentNode->OutputChannelCount(0), Muted ? 0.f : float(Level), smooth_time_frames);
    ma_result result = ma_gainer_node_init(ParentNode->Graph->Get(), &config, nullptr, Get());
    if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize gainer node: {}", int(result))); }
}

void AudioGraphNode::GainerNode::Uninit() {
    ma_gainer_node_uninit(Get(), nullptr);
}

void AudioGraphNode::GainerNode::OnComponentChanged() {
    if (Smooth.IsChanged()) {
        Uninit();
        Init();
        ParentNode->NotifyConnectionsChanged();
    }
    if (Muted.IsChanged() || Level.IsChanged()) UpdateLevel();
}

ma_gainer_node *AudioGraphNode::GainerNode::Get() { return Gainer.get(); }

void AudioGraphNode::GainerNode::UpdateLevel() {
    ma_gainer_node_set_gain(Get(), Muted ? 0.f : float(Level));
}

void AudioGraphNode::GainerNode::SetMuted(bool muted) {
    Muted.Set_(muted);
    UpdateLevel();
}

void AudioGraphNode::GainerNode::SetSampleRate(u32 sample_rate) {
    if (SampleRate != sample_rate) {
        SampleRate = sample_rate;
        Uninit();
        Init();
        ParentNode->NotifyConnectionsChanged();
    }
}

void AudioGraphNode::GainerNode::Render() const {
    Muted.Draw();
    Level.Draw();
    Smooth.Draw();
}

AudioGraphNode::PannerNode::PannerNode(ComponentArgs &&args)
    : Component(std::move(args)), ParentNode(static_cast<AudioGraphNode *>(Parent->Parent)) {
    Pan.RegisterChangeListener(this);
    Mode.RegisterChangeListener(this);

    Panner = std::make_unique<ma_panner_node>();

    auto config = ma_panner_node_config_init(ParentNode->OutputChannelCount(0));
    ma_result result = ma_panner_node_init(ParentNode->Graph->Get(), &config, nullptr, Get());
    if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize panner node: {}", int(result))); }
    UpdatePan();
    UpdateMode();
}

AudioGraphNode::PannerNode::~PannerNode() {
    ma_panner_node_uninit(Get(), nullptr);
    UnregisterChangeListener(this);
}

void AudioGraphNode::PannerNode::OnComponentChanged() {
    if (Pan.IsChanged()) UpdatePan();
    if (Mode.IsChanged()) UpdateMode();
}

ma_panner_node *AudioGraphNode::PannerNode::Get() { return Panner.get(); }

void AudioGraphNode::PannerNode::UpdatePan() { ma_panner_node_set_pan(Get(), float(Pan)); }
void AudioGraphNode::PannerNode::UpdateMode() { ma_panner_node_set_mode(Get(), ma_pan_mode(int(Mode))); }

void AudioGraphNode::PannerNode::Render() const {
    Pan.Draw();
    Mode.Draw();
}

static WindowFunctionType GetWindowFunction(WindowType type) {
    switch (type) {
        case WindowType_Rectangular: return rectwin;
        case WindowType_Hann: return hann_periodic;
        case WindowType_Hamming: return hamming_periodic;
        case WindowType_Blackman: return blackman_periodic;
        case WindowType_BlackmanHarris: return blackmanharris_periodic;
        case WindowType_Nuttall: return nuttallwin_periodic;
        case WindowType_FlatTop: return flattopwin_periodic;
        case WindowType_Triangular: return triang;
        case WindowType_Bartlett: return bartlett;
        case WindowType_BartlettHann: return barthannwin;
        case WindowType_Bohman: return bohmanwin;
        case WindowType_Parzen: return parzenwin;
        default: return nullptr;
    }
}

AudioGraphNode::MonitorNode::MonitorNode(ComponentArgs &&args)
    : Component(std::move(args)), ParentNode(static_cast<AudioGraphNode *>(Parent->Parent)),
      Type(PathSegment.starts_with(to_string(IO_In)) ? IO_In : IO_Out) {
    Component::References listening_to = {WindowType, WindowLength};
    for (const auto &component : listening_to) component.get().RegisterChangeListener(this);

    Monitor = std::make_unique<ma_monitor_node>();
    Init();
}

AudioGraphNode::MonitorNode::~MonitorNode() {
    Uninit();
    UnregisterChangeListener(this);
}

void AudioGraphNode::MonitorNode::Init() {
    auto config = ma_monitor_node_config_init(ParentNode->ChannelCount(Type, 0), WindowLength);
    ma_result result = ma_monitor_node_init(ParentNode->Graph->Get(), &config, nullptr, Get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize monitor node: {}", int(result)));

    UpdateWindowType();
}

void AudioGraphNode::MonitorNode::Uninit() {
    ma_monitor_node_uninit(Get(), nullptr);
}

void AudioGraphNode::MonitorNode::OnComponentChanged() {
    if (WindowType.IsChanged()) UpdateWindowType();
    if (WindowLength.IsChanged()) UpdateWindowLength();
}

void AudioGraphNode::MonitorNode::UpdateWindowType() {
    auto window_function = GetWindowFunction(WindowType);
    if (window_function == nullptr) throw std::runtime_error(std::format("Failed to get window function for window type {}.", int(WindowType)));

    ApplyWindowFunction(std::move(window_function));
}

void AudioGraphNode::MonitorNode::UpdateWindowLength() {
    // Recreate the monitor node to update the buffer size.
    Uninit();
    Init();
    ParentNode->NotifyConnectionsChanged();
}

ma_monitor_node *AudioGraphNode::MonitorNode::Get() { return Monitor.get(); }

std::string AudioGraphNode::MonitorNode::GetWindowLengthName(u32 window_length_frames) const {
    return std::format("{} ({:.2f} ms)", window_length_frames, float(window_length_frames * 1000) / float(ParentNode->Graph->SampleRate));
}

void AudioGraphNode::MonitorNode::ApplyWindowFunction(WindowFunctionType window_function) {
    ma_monitor_apply_window_function(Get(), window_function);
}

void AudioGraphNode::MonitorNode::RenderWaveform() const {
    if (ImPlot::BeginPlot("Waveform", {-1, 160})) {
        const auto N = Monitor->config.buffer_frames;
        ImPlot::SetupAxes("Frame", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, N, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
        if (ParentNode->IsActive) {
            for (u32 channel_index = 0; channel_index < Monitor->config.channels; channel_index++) {
                const std::string channel_name = std::format("Channel {}", channel_index);
                ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                ImPlot::PlotLine(channel_name.c_str(), Monitor->buffer, N);
                ImPlot::PopStyleVar();
            }
        }
        ImPlot::EndPlot();
    }
}

void AudioGraphNode::MonitorNode::RenderMagnitudeSpectrum() const {
    if (ImPlot::BeginPlot("Magnitude spectrum", {-1, 160})) {
        static const float MIN_DB = -100;
        const fft_data *fft = Monitor->fft;
        const u32 N = Monitor->config.buffer_frames;
        const u32 N_2 = N / 2;
        const float fs = ParentNode->Graph->SampleRate;
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
        if (ParentNode->IsActive) {
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

void AudioGraphNode::MonitorNode::Render() const {
    static const std::vector<u32> WindowLengthOptions = {256, 512, 1024, 2048, 4096, 8192, 16384};
    SetNextItemWidth(GetFontSize() * 9);
    WindowLength.Render(WindowLengthOptions);
    SetNextItemWidth(GetFontSize() * 9);
    WindowType.Draw();
    RenderWaveform();
    RenderMagnitudeSpectrum();
}

struct AudioGraphNode::SplitterNode {
    SplitterNode(ma_node_graph *ma_graph, u32 buses, u32 channels) {
        auto config = ma_splitter_node_config_init(channels);
        config.outputBusCount = buses;
        ma_result result = ma_splitter_node_init(ma_graph, &config, nullptr, &Splitter);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", int(result)));
    }
    ~SplitterNode() {
        ma_splitter_node_uninit(&Splitter, nullptr);
    }

    auto *Get() noexcept { return &Splitter; }

private:
    ma_splitter_node Splitter;
};

AudioGraphNode::AudioGraphNode(ComponentArgs &&args, CreateNodeFunction create_node)
    : Component(std::move(args)), Graph(static_cast<AudioGraph *>(Name == "Audio graph" ? this : Parent->Parent)), Node(create_node()) {
    Component::References listening_to = {Graph->SampleRate, InputGainer, OutputGainer, Panner, InputMonitor, OutputMonitor};
    for (const auto &component : listening_to) component.get().RegisterChangeListener(this);
}

AudioGraphNode::~AudioGraphNode() {
    DisconnectOutput();
    Splitter.reset();
    InputGainer.Reset();
    InputMonitor.Reset();
    OutputGainer.Reset();
    Panner.Reset();
    OutputMonitor.Reset();

    Listeners.clear();
    UnregisterChangeListener(this);
}

ma_node *AudioGraphNode::InputNode() const {
    if (InputGainer) return InputGainer->Get();
    if (InputMonitor) return InputMonitor->Get();
    return Get();
}
ma_node *AudioGraphNode::OutputNode() const {
    if (OutputMonitor) return OutputMonitor->Get();
    if (Panner) return Panner->Get();
    if (OutputGainer) return OutputGainer->Get();
    return Get();
}

AudioGraphNode::GainerNode *AudioGraphNode::GetGainerNode(IO io) const {
    const auto &gainer = GetGainer(io);
    return gainer ? gainer.Get() : nullptr;
}

AudioGraphNode::PannerNode *AudioGraphNode::GetPannerNode() const { return Panner ? Panner.Get() : nullptr; }

AudioGraphNode::MonitorNode *AudioGraphNode::GetMonitorNode(IO io) const {
    const auto &monitor = GetMonitor(io);
    return monitor ? monitor.Get() : nullptr;
}

void AudioGraphNode::OnSampleRateChanged() {
    if (InputGainer) InputGainer->SetSampleRate(Graph->SampleRate);
    if (OutputGainer) OutputGainer->SetSampleRate(Graph->SampleRate);
}

void AudioGraphNode::OnComponentChanged() {
    if (Graph->SampleRate.IsChanged()) OnSampleRateChanged();
    if (InputGainer.IsChanged() || OutputGainer.IsChanged() || Panner.IsChanged() || InputMonitor.IsChanged() || OutputMonitor.IsChanged()) {
        NotifyConnectionsChanged(); // An inner node was added/removed.
    }
}

u32 AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Get()); }

// Technically, the graph endpoint node has an output bus, but it's handled specially by miniaudio.
// Most importantly, it is not possible to attach the graph endpoint's node into any other node.
// Thus, we treat it strictly as a sink and hide the fact that it technically has an output bus, since it functionally does not.
u32 AudioGraphNode::OutputBusCount() const { return IsGraphEndpoint() ? 0 : ma_node_get_output_bus_count(Get()); }

u32 AudioGraphNode::InputChannelCount(u32 bus) const { return ma_node_get_input_channels(Get(), bus); }
u32 AudioGraphNode::OutputChannelCount(u32 bus) const { return ma_node_get_output_channels(Get(), bus); }

void AudioGraphNode::DisconnectOutput() {
    ma_node_detach_all_output_buses(OutputNode());
    Splitter.reset();
}

ma_node *AudioGraphNode::CreateSplitter(u32 destination_count) {
    Splitter = std::make_unique<SplitterNode>(Graph->Get(), destination_count, OutputChannelCount(0));
    return Splitter->Get();
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
    const bool is_io_node = InputBusCount() > 0 && OutputBusCount() > 0;
    for (IO io : IO_All) {
        if (BusCount(io) > 0) {
            const std::string label = is_io_node ? std::format("{} level", StringHelper::Capitalize(to_string(io))) : "Level";
            if (TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto &gainer = GetGainer(io);
                bool bypass = !gainer;
                if (Checkbox("Bypass", &bypass)) gainer.IssueToggle();
                if (gainer) gainer->Draw();
                TreePop();
            }
        }
    }
    if (BusCount(IO_Out) > 0) {
        if (TreeNodeEx("Panner", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool bypass = !Panner;
            if (Checkbox("Bypass", &bypass)) Panner.IssueToggle();
            if (Panner) Panner->Draw();
            TreePop();
        }
    }
    for (IO io : IO_All) {
        if (BusCount(io) > 0) {
            const std::string label = is_io_node ? std::format("{} monitor", StringHelper::Capitalize(to_string(io))) : "Monitor";
            if (TreeNode(label.c_str())) {
                const auto &monitor = GetMonitor(io);
                bool has_monitor = monitor;
                if (Checkbox("Monitor", &has_monitor)) monitor.IssueToggle();
                if (monitor) monitor->Draw();
                TreePop();
            }
        }
    }
    if (ImGui::TreeNode("Node info")) {
        TextUnformatted(std::format("Active: {}", bool(IsActive)));
        SameLine();
        fg::HelpMarker(
            "A node is considered active if there is a connection path from it to the graph endpoint node.\n"
            "Inactive nodes are not processed by the audio graph, and thus do not contribute to CPU load."
        );
        SeparatorText("Buses");
        for (IO io : IO_All) {
            Text("%s buses (%u)", StringHelper::Capitalize(to_string(io)).c_str(), BusCount(io));
            for (u32 bus = 0; bus < BusCount(io); bus++) {
                const u32 channels = ChannelCount(io, bus);
                BulletText("Bus %u: %u channel%s", bus, channels, channels == 1 ? "" : "s");
            }
        }
        SeparatorText("Connections");
        const auto source_nodes = Graph->GetSourceNodes(this), destination_nodes = Graph->GetDestinationNodes(this);
        if (!source_nodes.empty() || !destination_nodes.empty()) {
            Text("%s%s%s", NodesToString(source_nodes, true).c_str(), Name.c_str(), NodesToString(destination_nodes, false).c_str());
        } else {
            TextUnformatted("No connections");
        }
        SeparatorText("Internal nodes");
        for (IO io : IO_All) {
            if (BusCount(io) > 0) {
                const string io_label = StringHelper::Capitalize(to_string(io));
                BulletText("%s", std::format("{} gainer: {}", io_label, bool(GetGainer(io))).c_str());
                if (io == IO_Out) BulletText("%s", std::format("{} panner: {}", io_label, bool(Panner)).c_str());
                BulletText("%s", std::format("{} monitor: {}", io_label, bool(GetMonitor(io))).c_str());
                if (io == IO_Out) BulletText("%s", std::format("{} splitter: {}", io_label, bool(Splitter)).c_str());
            }
        }
        TreePop();
    }
}
