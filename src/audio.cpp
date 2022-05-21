// Adapted from:
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <soundio/soundio.h>

#include "context.h"
#include "stateful_imgui.h"

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

    auto &settings = s.audio.settings;
    int err = (settings.backend == none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, getSoundIOBackend(settings.backend));
    if (err) throw std::runtime_error(std::string("Unable to connect to backend: ") + soundio_strerror(err));

    std::cout << "Backend: " << soundio_backend_name(soundio->current_backend) << std::endl;

    soundio_flush_events(soundio);

    // Output device setup
    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) throw std::runtime_error("No output device found");

    int out_device_index = default_out_device_index;
    if (settings.out_device_id) {
        bool found = false;
        for (int i = 0; i < soundio_output_device_count(soundio); i += 1) {
            struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
            if (device->is_raw == settings.out_raw && strcmp(device->id, settings.out_device_id) == 0) {
                out_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found) throw std::runtime_error(std::string("Invalid output device id: ") + settings.out_device_id);
    }

    struct SoundIoDevice *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device) throw std::runtime_error("Could not get output device: out of memory");
    if (out_device->probe_error) throw std::runtime_error(std::string("Cannot probe device: ") + soundio_strerror(out_device->probe_error));

    auto *outstream = soundio_outstream_create(out_device);
    if (!outstream) throw std::runtime_error("Out of memory");

    std::cout << "Output device: " << out_device->name << std::endl;
    outstream->software_latency = settings.latency;

    int default_sample_rate = settings.sample_rate;
    int *sample_rate = &default_sample_rate;
    if (*sample_rate != 0) {
        if (!soundio_device_supports_sample_rate(out_device, *sample_rate)) {
            throw std::runtime_error("Output audio device does not support the provided sample rate of " + std::to_string(*sample_rate));
        }
    } else {
        for (sample_rate = prioritized_sample_rates; *sample_rate; sample_rate += 1) {
            if (soundio_device_supports_sample_rate(out_device, *sample_rate)) break;
        }
        if (!*sample_rate) throw std::runtime_error("Output audio device does not support any of the sample rates in `prioritized_sample_rates`.");
    }

    outstream->sample_rate = *sample_rate;

    enum SoundIoFormat *format;
    for (format = prioritized_formats; *format != SoundIoFormatInvalid; format += 1) {
        if (soundio_device_supports_format(out_device, *format)) break;
    }
    if (*format == SoundIoFormatInvalid) throw std::runtime_error("No suitable device format available");

    write_sample = write_sample_for_format(*format);
    q(set_audio_sample_rate{outstream->sample_rate});

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
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
            c.compute_frames(frame_count);

            const auto *layout = &outstream->layout;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int channel = 0; channel < layout->channel_count; channel += 1) {
                    write_sample(areas[channel].ptr, c.get_sample(channel, frame));
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

    while (s.processes.audio.running) {}

    // SoundIO cleanup
    soundio_outstream_destroy(outstream);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);

    return 0;
}

void Audio::draw() {
    StatefulImGui::DrawWindow(ui_s.audio.settings);
    StatefulImGui::DrawWindow(ui_s.audio.faust.editor, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(ui_s.audio.faust.log);
}
