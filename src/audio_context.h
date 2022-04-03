#pragma once

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
        float **input{};
        float **output{};

        FaustBuffers(int num_input_channels, int num_output_channels) :
            num_input_channels(num_input_channels), num_output_channels(num_output_channels) {
            input = new float *[num_input_channels];
            output = new float *[num_output_channels];
            for (int i = 0; i < num_input_channels; i++) { input[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
            for (int i = 0; i < num_output_channels; i++) { output[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
        }

        ~FaustBuffers() {
            for (int i = 0; i < num_input_channels; i++) { delete[] input[i]; }
            for (int i = 0; i < num_output_channels; i++) { delete[] output[i]; }
            delete input;
            delete output;
        }
    };

    AudioContext() = default;

    static void compute(int frame_count);
    static void on_action(const Action &);
    static float get_sample(int channel, int frame);
private:
    static void update();
};
