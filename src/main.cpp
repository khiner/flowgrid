// Adapted from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_sine.c

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <thread>
#include <soundio/soundio.h>

#include "context.h"
#include "draw.h"

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

static void (*write_sample)(char *ptr, double sample);

static const double PI = 3.14159265358979323846264338328;
static double seconds_offset = 0.0;

static Context context{};
static auto &state = context.state;

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

static int audioMain(AudioConfig config) {
    auto soundIOBackend = getSoundIOBackend(config.backend);
    auto *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int err = (config.backend == none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, soundIOBackend);
    if (err) {
        fprintf(stderr, "Unable to connect to backend: %s\n", soundio_strerror(err));
        return 1;
    }

    fprintf(stderr, "Backend: %s\n", soundio_backend_name(soundio->current_backend));

    soundio_flush_events(soundio);

    int selected_device_index = -1;
    if (config.device_id) {
        int device_count = soundio_output_device_count(soundio);
        for (int i = 0; i < device_count; i += 1) {
            auto *device = soundio_get_output_device(soundio, i);
            bool select_this_one = strcmp(device->id, config.device_id) == 0 && device->is_raw == config.raw;
            soundio_device_unref(device);
            if (select_this_one) {
                selected_device_index = i;
                break;
            }
        }
    } else {
        selected_device_index = soundio_default_output_device_index(soundio);
    }

    if (selected_device_index < 0) {
        fprintf(stderr, "Output device not found\n");
        return 1;
    }

    auto *device = soundio_get_output_device(soundio, selected_device_index);
    if (!device) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    fprintf(stderr, "Output device: %s\n", device->name);

    if (device->probe_error) {
        fprintf(stderr, "Cannot probe device: %s\n", soundio_strerror(device->probe_error));
        return 1;
    }

    auto *outstream = soundio_outstream_create(device);
    if (!outstream) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    outstream->name = config.stream_name;
    outstream->software_latency = config.latency;
    outstream->sample_rate = config.sample_rate;

    if (soundio_device_supports_format(device, SoundIoFormatFloat32NE)) {
        outstream->format = SoundIoFormatFloat32NE;
        write_sample = write_sample_float32ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatFloat64NE)) {
        outstream->format = SoundIoFormatFloat64NE;
        write_sample = write_sample_float64ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatS32NE)) {
        outstream->format = SoundIoFormatS32NE;
        write_sample = write_sample_s32ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatS16NE)) {
        outstream->format = SoundIoFormatS16NE;
        write_sample = write_sample_s16ne;
    } else {
        fprintf(stderr, "No suitable device format available.\n");
        return 1;
    }

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        double float_sample_rate = outstream->sample_rate;
        double seconds_per_frame = 1.0 / float_sample_rate;
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

            const auto *layout = &outstream->layout;
            double radians_per_second = state.sine_frequency * 2.0 * PI;
            for (int frame = 0; frame < frame_count; frame += 1) {
                double sample = state.sine_on ? state.sine_amplitude * sin((seconds_offset + frame * seconds_per_frame) * radians_per_second) : 0.0f;
                for (int channel = 0; channel < layout->channel_count; channel += 1) {
                    write_sample(areas[channel].ptr, sample);
                    areas[channel].ptr += areas[channel].step;
                }
            }
            seconds_offset = fmod(seconds_offset + seconds_per_frame * frame_count, 1.0);

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

    while (state.audio_engine_running) {}

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);

    return 0;
}

int main(int, char **) {
    std::thread audio_thread(audioMain, state.audio_config);
    draw(context);
    audio_thread.join();
    return 0;
}
