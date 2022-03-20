// Adapted from:
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <iostream>
#include <soundio/soundio.h>
#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend

#include "config.h"
#include "context.h"

static int prioritized_sample_rates[] = {
    48000,
    44100,
    96000,
    24000,
    0,
};

static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat64NE,
    SoundIoFormatS32NE,
    SoundIoFormatS16NE,
    SoundIoFormatInvalid,
};

SoundIoBackend getSoundIOBackend(AudioBackend backend) {
    switch (backend) {
        case dummy: return SoundIoBackendDummy;
        case alsa: return SoundIoBackendAlsa;
        case pulseaudio: return SoundIoBackendPulseAudio;
        case jack: return SoundIoBackendJack;
        case coreaudio: return SoundIoBackendCoreAudio;
        case wasapi: return SoundIoBackendWasapi;
        case none:
        default:std::cerr << "Invalid backend: " << backend << ". Defaulting to `SoundIoBackendNone`." << std::endl;
            return SoundIoBackendNone;
    }
}

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

struct FaustLlvmDsp {
    llvm_dsp_factory *dsp_factory;
    dsp *dsp;

    explicit FaustLlvmDsp(int sample_rate) {
        int argc = 0;
        const char **argv = new const char *[8];
        argv[argc++] = "-I";
        argv[argc++] = &config.faust_libraries_path[0]; // convert to char*
        // Consider additional args: "-vec", "-vs", "128", "-dfs"

        const int optimize = -1;
        std::string faust_error_msg;
        dsp_factory = createDSPFactoryFromString("FlowGrid", s.audio.faust_text, argc, argv, "", faust_error_msg, optimize);
        if (!faust_error_msg.empty()) throw std::runtime_error("[Faust]: " + faust_error_msg);

        dsp = dsp_factory->createDSPInstance();
        dsp->init(sample_rate);
    }

    ~FaustLlvmDsp() {
        delete dsp;
        deleteDSPFactory(dsp_factory);
    }
};

struct SoundIoStreamContext {
    FaustLlvmDsp dsp;
    FaustBuffers buffers;

    explicit SoundIoStreamContext(int sample_rate) : dsp(sample_rate), buffers(dsp.dsp->getNumInputs(), dsp.dsp->getNumOutputs()) {}
};

static void write_sample_s16ne(char *ptr, double sample) {
    auto *buf = (int16_t *) ptr;
    *buf = (int16_t) (sample * ((double) INT16_MAX - (double) INT16_MIN) / 2.0);
}

static void write_sample_s32ne(char *ptr, double sample) {
    auto *buf = (int32_t *) ptr;
    *buf = (int32_t) (sample * ((double) INT32_MAX - (double) INT32_MIN) / 2.0);
}

static void write_sample_float32ne(char *ptr, double sample) {
    auto *buf = (float *) ptr;
    *buf = (float) sample;
}

static void write_sample_float64ne(char *ptr, double sample) {
    auto *buf = (double *) ptr;
    *buf = sample;
}

auto write_sample_for_format(const SoundIoFormat format) {
    switch (format) {
        case SoundIoFormatFloat32NE: return write_sample_float32ne;
        case SoundIoFormatFloat64NE: return write_sample_float64ne;
        case SoundIoFormatS32NE: return write_sample_s32ne;
        case SoundIoFormatS16NE: return write_sample_s16ne;
        default: throw std::runtime_error(std::string("No `write_sample` function defined for format"));
    }
}

static void (*write_sample)(char *ptr, double sample); // Determined at runtime below.

int audio() {
    auto *soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = (s.audio.backend == none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, getSoundIOBackend(s.audio.backend));
    if (err) throw std::runtime_error(std::string("Unable to connect to backend: ") + soundio_strerror(err));

    std::cout << "Backend: " << soundio_backend_name(soundio->current_backend) << std::endl;

    soundio_flush_events(soundio);

    // Output device setup
    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) throw std::runtime_error("No output device found");

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
        if (!found) throw std::runtime_error(std::string("Invalid output device id: ") + s.audio.out_device_id);
    }

    struct SoundIoDevice *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device) throw std::runtime_error("Could not get output device: out of memory");
    if (out_device->probe_error) throw std::runtime_error(std::string("Cannot probe device: ") + soundio_strerror(out_device->probe_error));

    auto *outstream = soundio_outstream_create(out_device);
    if (!outstream) throw std::runtime_error("Out of memory");

    std::cout << "Output device: " << out_device->name << std::endl;
    outstream->software_latency = s.audio.latency;

    int default_sample_rate = s.audio.sample_rate;
    int *sample_rate = &default_sample_rate;
    if (*sample_rate != 0) {
        if (!soundio_device_supports_sample_rate(out_device, *sample_rate)) {
            throw std::runtime_error("Output audio device does not support the provided sample rate of " + std::to_string(default_sample_rate));
        }
    } else {
        for (sample_rate = prioritized_sample_rates; *sample_rate; sample_rate += 1) {
            if (soundio_device_supports_sample_rate(out_device, *sample_rate)) break;
        }
        if (!*sample_rate) throw std::runtime_error("Output audio device does not support any of the sample rates in `prioritized_sample_wrates`.");
    }

    outstream->sample_rate = *sample_rate;

    enum SoundIoFormat *format;
    for (format = prioritized_formats; *format != SoundIoFormatInvalid; format += 1) {
        if (soundio_device_supports_format(out_device, *format)) break;
    }
    if (*format == SoundIoFormatInvalid) throw std::runtime_error("No suitable device format available");

    write_sample = write_sample_for_format(*format);

    SoundIoStreamContext stream_context{outstream->sample_rate};
    outstream->userdata = &stream_context;
    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        const auto *stream_context = reinterpret_cast<SoundIoStreamContext *>(outstream->userdata);
        const auto &buffers = stream_context->buffers;
        const auto &dsp = stream_context->dsp;
        struct SoundIoChannelArea *areas;
        int err;

        int frames_left = frame_count_max;
        while (true) {
            int frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                std::cerr << "Unrecoverable stream error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }
            if (!frame_count) break;
            if (frame_count > buffers.num_frames) {
                std::cerr << "The output stream buffer only has " << buffers.num_frames
                          << " frames, which is smaller than the libsoundio callback buffer size of " << frame_count << "." << std::endl
                          << "(Increase `MAX_EXPECTED_FRAME_COUNT`.)" << std::endl;
                exit(1);
            }

            dsp.dsp->compute(frame_count, buffers.input, buffers.output);

            const auto *layout = &outstream->layout;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int channel = 0; channel < layout->channel_count; channel += 1) {
                    if (channel < buffers.num_output_channels) {
                        write_sample(areas[channel].ptr, s.audio.muted ? 0.0 : buffers.output[channel][frame]);
                    }
                    areas[channel].ptr += areas[channel].step;
                }
            }

            if ((err = soundio_outstream_end_write(outstream))) {
                if (err == SoundIoErrorUnderflow) return;
                std::cerr << "Unrecoverable stream error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }

            frames_left -= frame_count;
            if (frames_left <= 0) break;
        }
    };

    outstream->underflow_callback = [](SoundIoOutStream *) {
        static int underflow_count = 0;
        std::cerr << "Underflow #" << underflow_count++ << std::endl;
    };

    if ((err = soundio_outstream_open(outstream))) {
        throw std::runtime_error(std::string("Unable to open device: ") + soundio_strerror(err));
    }
    if (outstream->layout_error) {
        std::cerr << "Unable to set channel layout: " << soundio_strerror(outstream->layout_error);
    }
    if ((err = soundio_outstream_start(outstream))) {
        throw std::runtime_error(std::string("Unable to start device: ") + soundio_strerror(err));
    }

    std::cout << "Software latency (s): " << outstream->software_latency << std::endl;

    while (s.audio.running) {}

    // SoundIO cleanup
    soundio_outstream_destroy(outstream);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);

    return 0;
}
