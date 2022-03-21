#include "audio_context.h"
#include "context.h"

#include <utility>

void AudioContext::on_action(const Action &) {
    update();
}

void AudioContext::compute(int frame_count) const {
    if (dsp && buffers) dsp->dsp->compute(frame_count, buffers->input, buffers->output);
}

FAUSTFLOAT AudioContext::get_sample(int channel, int frame) const {
    if (!buffers || s.audio.muted) return 0;
    return buffers->output[std::min(channel, buffers->num_output_channels - 1)][frame];
}

void AudioContext::update() {
    if (!dsp || dsp->dsp->getSampleRate() != s.audio.sample_rate || dsp->faust_text != s.audio.faust.code) {
        dsp = std::make_unique<FaustLlvmDsp>(s.audio.sample_rate, s.audio.faust.code);
        buffers = std::make_unique<FaustBuffers>(dsp->dsp->getNumInputs(), dsp->dsp->getNumOutputs());
    }
}

AudioContext::FaustLlvmDsp::FaustLlvmDsp(int sample_rate, std::string faust_text) : faust_text(std::move(faust_text)) {
    int argc = 0;
    const char **argv = new const char *[8];
    argv[argc++] = "-I";
    argv[argc++] = &config.faust_libraries_path[0]; // convert to char*
    // Consider additional args: "-vec", "-vs", "128", "-dfs"

    const int optimize = -1;
    std::string faust_error_msg;
    dsp_factory = createDSPFactoryFromString("FlowGrid", this->faust_text, argc, argv, "", faust_error_msg, optimize);
    if (!faust_error_msg.empty()) throw std::runtime_error("[Faust]: " + faust_error_msg);

    dsp = dsp_factory->createDSPInstance();
    dsp->init(sample_rate);
}

AudioContext::FaustLlvmDsp::~FaustLlvmDsp() {
    delete dsp;
    deleteDSPFactory(dsp_factory);
}
