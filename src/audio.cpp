// Adapted from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_sine.c

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <soundio/soundio.h>
#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend

#include "context.h"


static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat64NE,
    SoundIoFormatS32NE,
    SoundIoFormatS16NE,
    SoundIoFormatInvalid,
};

static void write_sample_s16ne(char *ptr, double sample) {
    auto *buf = (int16_t *) ptr;
    double range = (double) INT16_MAX - (double) INT16_MIN;
    double val = sample * range / 2.0;
    *buf = (int16_t) val;
}

static void write_sample_s32ne(char *ptr, double sample) {
    auto *buf = (int32_t *) ptr;
    double range = (double) INT32_MAX - (double) INT32_MIN;
    double val = sample * range / 2.0;
    *buf = (int32_t) val;
}

static void write_sample_float32ne(char *ptr, double sample) {
    auto *buf = (float *) ptr;
    *buf = (float) sample;
}

static void write_sample_float64ne(char *ptr, double sample) {
    auto *buf = (double *) ptr;
    *buf = sample;
}

static void invalid_write_sample(char *, double) {}

static void (*write_sample)(char *ptr, double sample);

SoundIoBackend getSoundIOBackend(AudioBackend backend) {
    switch (backend) {
        case dummy: return SoundIoBackendDummy;
        case alsa: return SoundIoBackendAlsa;
        case pulseaudio: return SoundIoBackendPulseAudio;
        case jack: return SoundIoBackendJack;
        case coreaudio: return SoundIoBackendCoreAudio;
        case wasapi: return SoundIoBackendWasapi;
        case none:
        default:
            // XXX Will print a number for `backend`, not a name, I believe.
            fprintf(stderr, "Invalid backend: %u. Defaulting to SoundIoBackendNone\n", backend);
            return SoundIoBackendNone;
    }
}

struct FaustData {
    int num_frames;
    dsp *llvm_dsp;
    FAUSTFLOAT **input_samples;
    FAUSTFLOAT **output_samples;
};

auto write_sample_for_format(const SoundIoFormat format) {
    switch (format) {
        case SoundIoFormatFloat32NE: return write_sample_float32ne;
        case SoundIoFormatFloat64NE: return write_sample_float64ne;
        case SoundIoFormatS32NE: return write_sample_s32ne;
        case SoundIoFormatS16NE: return write_sample_s16ne;
        default: return invalid_write_sample;
    }
}

int audio(const std::string &faust_libraries_path) {
    auto &s = context.state;

    // Faust initialization
    int faust_argc = 0;
    const char **faust_argv = new const char *[8];
    faust_argv[faust_argc++] = "-I";
    faust_argv[faust_argc++] = &faust_libraries_path[0]; // convert to char*
//    argv[argc++] = "-vec";
//    argv[argc++] = "-vs";
//    argv[argc++] = "128";
//    argv[argc++] = "-dfs";
    const int optimize = -1;
    const std::string faust_code = "import(\"stdfaust.lib\"); process = no.noise;";
    std::string faust_error_msg = "Encountered an error during Faust DSP factory creation";
    auto *faust_dsp_factory = createDSPFactoryFromString("FlowGrid", faust_code, faust_argc, faust_argv, "", faust_error_msg, optimize);
    auto *faust_dsp = faust_dsp_factory->createDSPInstance();
    faust_dsp->init(context.state.audio.sample_rate);

    auto soundIOBackend = getSoundIOBackend(s.audio.backend);
    auto *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int err = (s.audio.backend == none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, soundIOBackend);
    if (err) {
        fprintf(stderr, "Unable to connect to backend: %s\n", soundio_strerror(err));
        return 1;
    }

    fprintf(stderr, "Backend: %s\n", soundio_backend_name(soundio->current_backend));

    soundio_flush_events(soundio);

    // Output device setup
    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) {
        fprintf(stderr, "No output device found");
        return 1;
    }
    int out_device_index = default_out_device_index;
    if (s.audio.out_device_id) {
        bool found = false;
        for (int i = 0; i < soundio_output_device_count(soundio); i += 1) {
            struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
            if (device->is_raw == s.audio.out_raw && strcmp(device->id, s.audio.out_device_id) == 0) {
                out_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found) {
            fprintf(stderr, "Invalid output device id: %s", s.audio.out_device_id);
            return 1;
        }
    }

    struct SoundIoDevice *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device) {
        fprintf(stderr, "Could not get output device: out of memory");
        return 1;
    }

    fprintf(stderr, "Output device: %s\n", out_device->name);

    if (out_device->probe_error) {
        fprintf(stderr, "Cannot probe device: %s\n", soundio_strerror(out_device->probe_error));
        return 1;
    }

    auto *outstream = soundio_outstream_create(out_device);
    if (!outstream) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    outstream->software_latency = s.audio.latency;
    outstream->sample_rate = s.audio.sample_rate;

    enum SoundIoFormat *format;
    for (format = prioritized_formats; *format != SoundIoFormatInvalid; format += 1) {
        if (soundio_device_supports_format(out_device, *format)) break;
    }
    if (*format == SoundIoFormatInvalid) {
        fprintf(stderr, "No suitable device format available.\n");
        return 1;
    }

    write_sample = write_sample_for_format(*format);
    if (write_sample == nullptr) {
        fprintf(stderr, "No `write_sample` function defined for format\n");
        return 1;
    }

    // TODO how can we get this outside of the write callback?
    //   (Maybe just make it as big as than the max possible?)
    const int expected_frame_count_max = 512;
    const int num_input_channels = faust_dsp->getNumInputs();
    const int num_output_channels = faust_dsp->getNumOutputs();
    FAUSTFLOAT *input_samples[num_input_channels];
    FAUSTFLOAT *output_samples[num_output_channels];
    for (int i = 0; i < num_input_channels; i++) { input_samples[i] = new FAUSTFLOAT[expected_frame_count_max]; }
    for (int i = 0; i < num_output_channels; i++) { output_samples[i] = new FAUSTFLOAT[expected_frame_count_max]; }

    FaustData faust_data{expected_frame_count_max, faust_dsp, input_samples, output_samples};
    outstream->userdata = &faust_data;

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        struct SoundIoChannelArea *areas;
        int err;

        int frames_left = frame_count_max;
        while (true) {
            int frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
                exit(1);
            }

            if (!frame_count) break;

            const auto *faust_data = reinterpret_cast<FaustData *>(outstream->userdata);
            faust_data->llvm_dsp->compute(frame_count, faust_data->input_samples, faust_data->output_samples);

            const auto *layout = &outstream->layout;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int channel = 0; channel < layout->channel_count; channel += 1) {
                    if (faust_data->num_frames && channel < faust_data->llvm_dsp->getNumOutputs()) {
                        write_sample(areas[channel].ptr, s.audio.muted ? 0.0 : faust_data->output_samples[channel][frame]);
                    }
                    areas[channel].ptr += areas[channel].step;
                }
            }

            if ((err = soundio_outstream_end_write(outstream))) {
                if (err == SoundIoErrorUnderflow) return;
                fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
                exit(1);
            }

            frames_left -= frame_count;
            if (frames_left <= 0) break;
        }
    };

    outstream->underflow_callback = [](SoundIoOutStream *) {
        static int count = 0;
        fprintf(stderr, "underflow %d\n", count++);
    };

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
        return 1;
    }

    fprintf(stdout, "Software latency: %f\n", outstream->software_latency);

    if (outstream->layout_error) {
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));
    }

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        return 1;
    }

    while (s.audio.running) {}

    // Faust cleanup
    for (int i = 0; i < num_input_channels; i++) { delete[] input_samples[i]; }
    for (int i = 0; i < num_output_channels; i++) { delete[] output_samples[i]; }

    delete faust_dsp;
    deleteDSPFactory(faust_dsp_factory);

    // SoundIO cleanup
    soundio_outstream_destroy(outstream);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);

    return 0;
}
