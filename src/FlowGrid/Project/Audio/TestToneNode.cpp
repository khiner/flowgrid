#include "Project/Audio/Graph/AudioGraph.h"

#include "Project/Audio/AudioDevice.h"

#include "imgui.h"
#include "miniaudio.h"

static ma_waveform *CurrentWaveform; // todo pass in `ma_node` userdata instead?

TestToneNode::TestToneNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    audio_device.SampleRate.RegisterChangeListener(this);
    Amplitude.RegisterChangeListener(this);
    Frequency.RegisterChangeListener(this);
    Type.RegisterChangeListener(this);
}

void TestToneNode::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();
    if (!CurrentWaveform) return;
    if (audio_device.SampleRate.IsChanged()) ma_waveform_set_sample_rate(CurrentWaveform, ma_uint32(audio_device.SampleRate));
    if (Amplitude.IsChanged()) ma_waveform_set_amplitude(CurrentWaveform, Amplitude);
    if (Frequency.IsChanged()) ma_waveform_set_frequency(CurrentWaveform, Frequency);
    if (Type.IsChanged()) ma_waveform_set_type(CurrentWaveform, ma_waveform_type(int(Type)));
    // MA_API ma_result ma_waveform_set_amplitude(ma_waveform* pWaveform, double amplitude);
    // MA_API ma_result ma_waveform_set_frequency(ma_waveform* pWaveform, double frequency);
    // MA_API ma_result ma_waveform_set_type(ma_waveform* pWaveform, ma_waveform_type type);
}

void Process(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    if (CurrentWaveform) ma_waveform_read_pcm_frames(CurrentWaveform, bus_frames_out[0], frame_count_out[0], nullptr);

    (void)node; /* Unused. */
    (void)const_bus_frames_in; /* Unused. */
    (void)frame_count_in; /* Unused. */
}

ma_node *TestToneNode::DoInit() {
    static ma_waveform waveform;
    ma_waveform_config waveform_config = ma_waveform_config_init(ma_format(int(audio_device.OutFormat)), audio_device.OutChannels, audio_device.SampleRate, ma_waveform_type(int(Type)), Amplitude, Frequency);
    int result = ma_waveform_init(&waveform_config, &waveform);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the TestTone waveform: {}", result));

    CurrentWaveform = &waveform;

    static ma_node_config config;
    config = ma_node_config_init();

    static const Count out_channels = audio_device.OutChannels;
    config.pOutputChannels = &out_channels;

    static ma_node_vtable vtable{};
    vtable = {Process, nullptr, 0, 1, 0};
    config.vtable = &vtable;

    static ma_node_base node{};
    result = ma_node_init(Graph->Get(), &config, nullptr, &node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the TestTone node: {}", result));

    return &node;
}

void TestToneNode::DoUninit() {
    if (!CurrentWaveform) return;

    ma_waveform_uninit(CurrentWaveform);
    CurrentWaveform = nullptr;
}

void TestToneNode::Render() const {
    AudioGraphNode::Render();

    ImGui::Spacing();
    Amplitude.Draw();
    Frequency.Draw();
    Type.Draw();
}
