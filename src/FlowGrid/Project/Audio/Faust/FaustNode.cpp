#include "Project/Audio/Graph/AudioGraph.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"
#include "Faust.h"
#include "Project/Audio/AudioDevice.h"

static dsp *CurrentDsp; // Only used in `FaustProcess`. todo pass in `ma_node` userdata instead?

void FaustNode::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();
    if (audio_device.SampleRate.IsChanged() && CurrentDsp) CurrentDsp->init(audio_device.SampleRate);
}

void FaustProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (CurrentDsp) CurrentDsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);

    (void)node; // unused
    (void)frame_count_in; // unused
}

void FaustNode::OnFaustDspChanged(dsp *dsp) {
    if (CurrentDsp && !dsp) {
        Uninit();
        CurrentDsp = nullptr;
    } else if (!CurrentDsp && dsp) {
        CurrentDsp = dsp;
        Init();
    } else {
        CurrentDsp = dsp;
        UpdateAll();
    }
}

ma_node *FaustNode::DoInit() {
    if (!CurrentDsp) return nullptr;

    CurrentDsp->init(audio_device.SampleRate);

    const Count in_channels = CurrentDsp->getNumInputs();
    const Count out_channels = CurrentDsp->getNumOutputs();
    if (in_channels == 0 && out_channels == 0) return nullptr;

    static ma_node_vtable vtable{};
    vtable = {FaustProcess, nullptr, ma_uint8(in_channels > 0 ? 1 : 0), ma_uint8(out_channels > 0 ? 1 : 0), 0};

    ma_node_config config = ma_node_config_init();
    config.pInputChannels = &in_channels; // One input bus with N channels.
    config.pOutputChannels = &out_channels; // One output bus with M channels.
    config.vtable = &vtable;

    static ma_node_base node{};
    const int result = ma_node_init(Graph->Get(), &config, nullptr, &node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust node: {}", result));

    return &node;
}
