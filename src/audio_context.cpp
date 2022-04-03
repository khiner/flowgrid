#include "audio_context.h"

#include <iostream>
#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend
#include "context.h"
#include <utility>

struct FaustContext {
    const std::string faust_text;
    int sample_rate;
    int num_inputs{0}, num_outputs{0};
    llvm_dsp_factory *dsp_factory;
    dsp *dsp = nullptr;
    std::unique_ptr<AudioContext::FaustBuffers> buffers;

    FaustContext(std::string faust_text, int sample_rate);
    ~FaustContext();

    void compute(int frame_count) const;
    FAUSTFLOAT
    get_sample(int channel, int frame) const;

    void update();
};

std::unique_ptr<FaustContext> faust;

void FaustContext::compute(int frame_count) const {
    if (buffers) {
        if (frame_count > buffers->num_frames) {
            std::cerr << "The output stream buffer only has " << buffers->num_frames
                      << " frames, which is smaller than the libsoundio callback buffer size of " << frame_count << "." << std::endl
                      << "(Increase `AudioContext.MAX_EXPECTED_FRAME_COUNT`.)" << std::endl;
            exit(1);
        }
        if (dsp) dsp->compute(frame_count, buffers->input, buffers->output);
    }
    // TODO log warning
}

FAUSTFLOAT
FaustContext::get_sample(int channel, int frame) const {
    if (!buffers || !dsp) return 0;
    return buffers->output[std::min(channel, buffers->num_output_channels - 1)][frame];
}

void FaustContext::update() {
    num_inputs = dsp ? dsp->getNumInputs() : 0;
    num_outputs = dsp ? dsp->getNumOutputs() : 0;
    buffers = std::make_unique<AudioContext::FaustBuffers>(num_inputs, num_outputs);
}


void AudioContext::on_action(const Action &) {
    update();
}

void AudioContext::compute(int frame_count) {
    if (faust) faust->compute(frame_count);
}

FAUSTFLOAT
AudioContext::get_sample(int channel, int frame) {
    return !faust || s.audio.muted ? 0 : faust->get_sample(channel, frame);
}

void AudioContext::update() {
    if (!faust || faust->sample_rate != s.audio.sample_rate || faust->faust_text != s.audio.faust.code) {
        faust = std::make_unique<FaustContext>(s.audio.faust.code, s.audio.sample_rate);
    }
}

FaustContext::FaustContext(std::string faust_text, int sample_rate)
    : faust_text(std::move(faust_text)), sample_rate(sample_rate) {
    int argc = 0;
    const char **argv = new const char *[8];
    argv[argc++] = "-I";
    argv[argc++] = &config.faust_libraries_path[0]; // convert to char*
    // Consider additional args: "-vec", "-vs", "128", "-dfs"

    const int optimize = -1;
    dsp_factory = createDSPFactoryFromString("FlowGrid", this->faust_text, argc, argv, "", c._state.audio.faust.error, optimize);
    if (c._state.audio.faust.error.empty()) {
        dsp = dsp_factory->createDSPInstance();
        dsp->init(sample_rate);
    }
    update();
}

FaustContext::~FaustContext() {
    if (dsp) {
        delete dsp;
        dsp = nullptr;
        deleteDSPFactory(dsp_factory);
    }
}
