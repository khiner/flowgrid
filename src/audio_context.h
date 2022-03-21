#pragma once

#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend
#include "config.h"
#include "action.h"

struct AudioContext {
    // Used to initialize the static Faust buffer.
    // This is the highest `max_frame_count` value I've seen coming into the output audio callback,
    // using a sample rate of 96kHz.
    // If it needs bumping up, bump away!
    static const int MAX_EXPECTED_FRAME_COUNT = 2048;

    struct FaustBuffers {
        const int num_frames = MAX_EXPECTED_FRAME_COUNT;
        const int num_input_channels;
        const int num_output_channels;
        FAUSTFLOAT **input;
        FAUSTFLOAT **output;

        FaustBuffers(int num_input_channels, int num_output_channels) :
            num_input_channels(num_input_channels), num_output_channels(num_output_channels) {
            input = new FAUSTFLOAT *[num_input_channels];
            output = new FAUSTFLOAT *[num_output_channels];
            for (int i = 0; i < num_input_channels; i++) { input[i] = new FAUSTFLOAT[MAX_EXPECTED_FRAME_COUNT]; }
            for (int i = 0; i < num_output_channels; i++) { output[i] = new FAUSTFLOAT[MAX_EXPECTED_FRAME_COUNT]; }
        }

        ~FaustBuffers() {
            for (int i = 0; i < num_input_channels; i++) { delete[] input[i]; }
            for (int i = 0; i < num_output_channels; i++) { delete[] output[i]; }
            delete input;
            delete output;
        }
    };

    struct FaustContext {
        const std::string faust_text;
        int sample_rate;
        int num_inputs{0}, num_outputs{0};
        llvm_dsp_factory *dsp_factory;
        dsp *dsp = nullptr;
        std::unique_ptr<FaustBuffers> buffers;

        FaustContext(std::string faust_text, int sample_rate);
        ~FaustContext();

        void compute(int frame_count) const;
        FAUSTFLOAT get_sample(int channel, int frame) const;

        void update();
    };

    std::unique_ptr<FaustContext> faust;

    AudioContext() = default;
    void on_action(const Action &);
    void compute(int frame_count) const;
    FAUSTFLOAT get_sample(int channel, int frame) const;
private:
    void update();
};
