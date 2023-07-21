#include "WaveformNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "miniaudio.h"

#include "imgui.h"

static ma_waveform *CurrentWaveform; // todo pass in `ma_node` userdata instead?

void Process(ma_node *node, const float **const_bus_frames_in, u32 *frame_count_in, float **bus_frames_out, u32 *frame_count_out) {
    if (CurrentWaveform) ma_waveform_read_pcm_frames(CurrentWaveform, bus_frames_out[0], frame_count_out[0], nullptr);

    (void)node; /* Unused. */
    (void)const_bus_frames_in; /* Unused. */
    (void)frame_count_in; /* Unused. */
}

WaveformNode::WaveformNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Frequency.RegisterChangeListener(this);
    Type.RegisterChangeListener(this);

    ma_waveform_config waveform_config = ma_waveform_config_init(ma_format_f32, 1, GetSampleRate(), ma_waveform_type(int(Type)), 1, Frequency);
    static ma_waveform waveform;
    int result = ma_waveform_init(&waveform_config, &waveform);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Waveform waveform: {}", result));

    CurrentWaveform = &waveform;

    static ma_node_vtable vtable = {Process, nullptr, 0, 1, 0};
    u32 out_channels = 1;
    ma_node_config config = ma_node_config_init();
    config.pOutputChannels = &out_channels;
    config.vtable = &vtable;

    static ma_node_base node{};
    result = ma_node_init(Graph->Get(), &config, nullptr, &node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Waveform node: {}", result));
    Node = &node;

    UpdateAll();
}

WaveformNode::~WaveformNode() {
    ma_waveform_uninit(CurrentWaveform);
    CurrentWaveform = nullptr;
}

void WaveformNode::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();
    if (!CurrentWaveform) return;

    if (Frequency.IsChanged()) ma_waveform_set_frequency(CurrentWaveform, Frequency);
    if (Type.IsChanged()) ma_waveform_set_type(CurrentWaveform, ma_waveform_type(int(Type)));
}

void WaveformNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    if (CurrentWaveform) ma_waveform_set_sample_rate(CurrentWaveform, GetSampleRate());
}

void WaveformNode::Render() const {
    AudioGraphNode::Render();

    ImGui::Spacing();
    Frequency.Draw();
    Type.Draw();
}
