// Adapted from:
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
//   * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <soundio/soundio.h>

#include "Context.h"

static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat64NE,
    SoundIoFormatS32NE,
    SoundIoFormatS16NE,
    SoundIoFormatInvalid,
};

SoundIoBackend soundio_backend(const Audio::Backend backend) {
    switch (backend) {
        case Audio::Backend::dummy: return SoundIoBackendDummy;
        case Audio::Backend::alsa: return SoundIoBackendAlsa;
        case Audio::Backend::pulseaudio: return SoundIoBackendPulseAudio;
        case Audio::Backend::jack: return SoundIoBackendJack;
        case Audio::Backend::coreaudio: return SoundIoBackendCoreAudio;
        case Audio::Backend::wasapi: return SoundIoBackendWasapi;
        case Audio::Backend::none:
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
        default: throw std::runtime_error(string("No `write_sample` function defined for format"));
    }
}

static void (*write_sample)(char *ptr, double sample); // Determined at runtime below.

SoundIo *soundio = nullptr;
SoundIoOutStream *outstream = nullptr;
std::vector<string> out_device_ids;
std::vector<int> device_sample_rates;

bool soundio_ready = false;
bool thread_running = false;
int underflow_count = 0;

int audio() {
    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    const auto &settings = s.audio.settings;
    int err = (settings.backend == Audio::Backend::none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, soundio_backend(settings.backend));
    if (err) throw std::runtime_error(string("Unable to connect to backend: ") + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Output device setup
    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) throw std::runtime_error("No output device found");

    out_device_ids.clear();
    for (int i = 0; i < soundio_output_device_count(soundio); i++) out_device_ids.emplace_back(soundio_get_output_device(soundio, i)->id);

    int out_device_index = default_out_device_index;
    if (settings.out_device_id) {
        bool found = false;
        for (int i = 0; i < soundio_output_device_count(soundio); i++) {
            auto *device = soundio_get_output_device(soundio, i);
            if (settings.out_device_id.value() == device->id) {
                out_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found) throw std::runtime_error(string("Invalid output device id: ") + settings.out_device_id.value());
    }

    auto *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device) throw std::runtime_error("Could not get output device: out of memory");
    if (out_device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(out_device->probe_error));

    outstream = soundio_outstream_create(out_device);
    if (!outstream) throw std::runtime_error("Out of memory");

    device_sample_rates.clear();
    for (int i = 0; i < out_device->sample_rate_count; i++) device_sample_rates.push_back(out_device->sample_rates[i].max);
    if (device_sample_rates.empty()) throw std::runtime_error("Output audio stream has no supported sample rates");

    // Could just check `device_sample_rates` populated above, but this `supports_sample_rate` function handles devices supporting ranges.
    if (soundio_device_supports_sample_rate(out_device, settings.sample_rate)) {
        outstream->sample_rate = settings.sample_rate;
    } else {
        for (const auto &preferred_sample_rate: Audio::PrioritizedDefaultSampleRates) {
            if (soundio_device_supports_sample_rate(out_device, preferred_sample_rate)) {
                outstream->sample_rate = preferred_sample_rate;
                break;
            }
        }
    }
    if (!outstream->sample_rate) outstream->sample_rate = device_sample_rates.back(); // Fall back to the highest supported sample rate.
    if (outstream->sample_rate != settings.sample_rate) q(set_value{s.audio.settings.path / "sample_rate", outstream->sample_rate});

    enum SoundIoFormat *format;
    for (format = prioritized_formats; *format != SoundIoFormatInvalid; format++) {
        if (soundio_device_supports_format(out_device, *format)) break;
    }
    if (*format == SoundIoFormatInvalid) throw std::runtime_error("No suitable device format available");
    outstream->format = *format;

    write_sample = write_sample_for_format(*format);

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        SoundIoChannelArea *areas;
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
            for (int frame = 0; frame < frame_count; frame++) {
                for (int channel = 0; channel < layout->channel_count; channel++) {
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

    outstream->underflow_callback = [](SoundIoOutStream *) { std::cerr << "Underflow #" << underflow_count++ << std::endl; };

    if ((err = soundio_outstream_open(outstream))) { throw std::runtime_error(string("Unable to open device: ") + soundio_strerror(err)); }
    if (outstream->layout_error) { std::cerr << "Unable to set channel layout: " << soundio_strerror(outstream->layout_error); }
    if ((err = soundio_outstream_start(outstream))) { throw std::runtime_error(string("Unable to start device: ") + soundio_strerror(err)); }

    soundio_ready = true;
    while (thread_running) {}

    // Cleanup
    soundio_ready = false;
    soundio_outstream_destroy(outstream);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);
    soundio = nullptr;

    return 0;
}

std::thread audio_thread;

void Audio::update_process() const {
    static int previous_sample_rate = s.audio.settings.sample_rate;

    if (thread_running != running) {
        thread_running = running;
        if (running) audio_thread = std::thread(audio);
        else audio_thread.join();
    }

    // Reset the audio thread to make any sample rate change take effect.
    if (thread_running && previous_sample_rate != s.audio.settings.sample_rate) {
        thread_running = false;
        audio_thread.join();
        thread_running = true;
        audio_thread = std::thread(audio);
    }
    previous_sample_rate = s.audio.settings.sample_rate;

    if (soundio_ready && outstream && outstream->volume != settings.device_volume) soundio_outstream_set_volume(outstream, settings.device_volume);
}

#include "UI/Widgets.h"

using namespace fg;
using namespace ImGui;

void ShowChannelLayout(const SoundIoChannelLayout &layout, bool is_current) {
    const char *current_str = is_current ? " (current)" : "";
    if (layout.name) Text("%s%s", layout.name, current_str);
    for (int i = 0; i < layout.channel_count; i++) {
        BulletText("%s", soundio_get_channel_name(layout.channels[i]));
    }
}

void ShowDevice(const SoundIoDevice &device, bool is_default) {
    const char *default_str = is_default ? " (default)" : "";
    const char *raw_str = device.is_raw ? " (raw)" : "";
    if (TreeNode("%s%s%s", device.name, default_str, raw_str)) {
        Text("ID: %s", device.id);
        if (device.probe_error) {
            Text("Probe error: %s", soundio_strerror(device.probe_error));
            return;
        }
        if (TreeNodeEx("Channel layouts", ImGuiTreeNodeFlags_DefaultOpen, "Channel layouts (%d)", device.layout_count)) {
            for (int i = 0; i < device.layout_count; i++) ShowChannelLayout(device.layouts[i], device.layouts[i].name == device.current_layout.name);
            TreePop();
        }
        if (TreeNodeEx("Sample rates", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int i = 0; i < device.sample_rate_count; i++) {
                const auto &range = device.sample_rates[i];
                if (range.min == range.max) BulletText("%d", range.min);
                else BulletText("Range: %d - %d", range.min, range.max);
            }
            TreePop();
        }

        if (TreeNodeEx("Formats", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int i = 0; i < device.format_count; i++) BulletText("%s", soundio_format_string(device.formats[i]));
            TreePop();
        }

        Text("Min software latency: %0.8f sec", device.software_latency_min);
        Text("Max software latency: %0.8f sec", device.software_latency_max);
        if (device.software_latency_current != 0.0) Text("Current software latency: %0.8f sec", device.software_latency_current);

        TreePop();
    }
}

// Based on https://github.com/andrewrk/libsoundio/blob/master/example/sio_list_devices.c
void ShowDevices() {
    const auto input_count = soundio_input_device_count(soundio);
    if (TreeNodeEx("Input devices", ImGuiTreeNodeFlags_DefaultOpen, "Input devices (%d)", input_count)) {
        const auto default_input = soundio_default_input_device_index(soundio);
        for (int i = 0; i < input_count; i++) {
            auto *device = soundio_get_input_device(soundio, i);
            ShowDevice(*device, default_input == i);
            soundio_device_unref(device);
        }
        TreePop();
    }

    const auto output_count = soundio_output_device_count(soundio);
    if (TreeNodeEx("Output devices", ImGuiTreeNodeFlags_DefaultOpen, "Output devices (%d)", output_count)) {
        const auto default_output = soundio_default_output_device_index(soundio);
        for (int i = 0; i < output_count; i++) {
            auto *device = soundio_get_output_device(soundio, i);
            ShowDevice(*device, default_output == i);
            soundio_device_unref(device);
        }
        TreePop();
    }
}

void ShowStreams() {
    if (TreeNodeEx("Output stream", ImGuiTreeNodeFlags_DefaultOpen)) {
        BulletText("Name: %s", outstream->name);
        BulletText("Device ID: %s", outstream->device->id);
        BulletText("Format: %s", soundio_format_string(outstream->format));
        BulletText("Sample rate: %d", outstream->sample_rate);
        if (TreeNodeEx("Channel layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ShowChannelLayout(outstream->layout, false);
            TreePop();
        }
        BulletText("Volume: %0.8f", outstream->volume);
        BulletText("Software latency: %0.8f sec", outstream->software_latency);
        BulletText("Bytes per frame: %d", outstream->bytes_per_frame);
        BulletText("Bytes per sample: %d", outstream->bytes_per_sample);

        TreePop();
    }
}

void ShowBackends() {
    const auto backend_count = soundio_backend_count(soundio);
    if (TreeNodeEx("Backends", ImGuiTreeNodeFlags_None, "Available backends (%d)", backend_count)) {
        for (int i = 0; i < backend_count; i++) {
            const auto backend = soundio_get_backend(soundio, i);
            BulletText("%s%s", soundio_backend_name(backend), backend == soundio->current_backend ? " (current)" : "");
        }
        TreePop();
    }

}
void Audio::Settings::draw() const {
    Checkbox(path.parent_pointer() / "running");
    Checkbox(path / "muted");
    SliderFloat(path / "device_volume", 0.0f, 1.0f);

    if (!out_device_ids.empty()) Combo(path / "out_device_id", out_device_ids);
    if (!device_sample_rates.empty()) Combo(path / "sample_rate", device_sample_rates);
    NewLine();
    if (!soundio_ready) {
        Text("No audio context created yet");
    } else {
        if (TreeNode("Devices")) {
            ShowDevices();
            TreePop();
        }
        if (TreeNode("Streams")) {
            ShowStreams();
            TreePop();
        }
        ShowBackends();
    }
}
