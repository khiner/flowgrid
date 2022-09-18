// Adapted from:
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <soundio/soundio.h>

#include "Context.h"

static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat64NE,
    SoundIoFormatS32NE,
    SoundIoFormatS16NE,
    SoundIoFormatInvalid,
};

SoundIoBackend soundio_backend(const AudioBackend backend) {
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

inline static FAUSTFLOAT read_sample_s16ne(const char *ptr) {
    const auto value = *(int16_t *) ptr;
    return 2.0f * FAUSTFLOAT(value) / (FAUSTFLOAT(INT16_MAX) - FAUSTFLOAT(INT16_MIN));
}
inline static FAUSTFLOAT read_sample_s32ne(const char *ptr) {
    const auto value = *(int32_t *) ptr;
    return 2.0f * FAUSTFLOAT(value) / (FAUSTFLOAT(INT32_MAX) - FAUSTFLOAT(INT32_MIN));
}
inline static FAUSTFLOAT read_sample_float32ne(const char *ptr) {
    const auto value = *(float *) ptr;
    return FAUSTFLOAT(value);
}
inline static FAUSTFLOAT read_sample_float64ne(const char *ptr) {
    const auto value = *(double *) ptr;
    return FAUSTFLOAT(value);
}

inline static void write_sample_s16ne(char *ptr, FAUSTFLOAT sample) {
    auto *buf = (int16_t *) ptr;
    *buf = int16_t(sample * (FAUSTFLOAT(INT16_MAX) - FAUSTFLOAT(INT16_MIN)) / 2.0);
}
inline static void write_sample_s32ne(char *ptr, FAUSTFLOAT sample) {
    auto *buf = (int32_t *) ptr;
    *buf = int32_t(sample * (FAUSTFLOAT(INT32_MAX) - FAUSTFLOAT(INT32_MIN)) / 2.0);
}
inline static void write_sample_float32ne(char *ptr, FAUSTFLOAT sample) {
    auto *buf = (float *) ptr;
    *buf = float(sample);
}
inline static void write_sample_float64ne(char *ptr, FAUSTFLOAT sample) {
    auto *buf = (double *) ptr;
    *buf = double(sample);
}

auto read_sample_for_format(const SoundIoFormat format) {
    switch (format) {
        case SoundIoFormatFloat32NE: return read_sample_float32ne;
        case SoundIoFormatFloat64NE: return read_sample_float64ne;
        case SoundIoFormatS32NE: return read_sample_s32ne;
        case SoundIoFormatS16NE: return read_sample_s16ne;
        default: throw std::runtime_error(string("No `read_sample` function defined for format"));
    }
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

// These IO read/write functions are determined at runtime below.
static FAUSTFLOAT (*read_sample)(const char *ptr);
static void (*write_sample)(char *ptr, FAUSTFLOAT sample);

SoundIo *soundio = nullptr;
SoundIoInStream *instream = nullptr;
SoundIoOutStream *outstream = nullptr;
std::map<IO, std::vector<string>> device_ids = {{IO_In, {}}, {IO_Out, {}}};
std::map<IO, std::vector<int>> device_sample_rates = {{IO_In, {}}, {IO_Out, {}}};

bool soundio_ready = false;
bool thread_running = false;
int underflow_count = 0;
int last_read_frame_count_max = 0;
int last_write_frame_count_max = 0;

int audio() {
    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = (s.Audio.Backend == AudioBackend::none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, soundio_backend(s.Audio.Backend));
    if (err) throw std::runtime_error(string("Unable to connect to backend: ") + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Input device setup
    int default_in_device_index = soundio_default_input_device_index(soundio);
    if (default_in_device_index < 0) throw std::runtime_error("No input device found"); // todo move on without input

    // Output device setup
    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) throw std::runtime_error("No output device found");

    device_ids[IO_In].clear();
    for (int i = 0; i < soundio_input_device_count(soundio); i++) device_ids[IO_In].emplace_back(soundio_get_input_device(soundio, i)->id);

    int in_device_index = default_in_device_index;
    if (s.Audio.InDeviceId) {
        bool found = false;
        for (int i = 0; i < soundio_input_device_count(soundio); i++) {
            auto *device = soundio_get_input_device(soundio, i);
            if (s.Audio.InDeviceId == device->id) {
                in_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found) throw std::runtime_error(string("Invalid input device id: ") + string(s.Audio.InDeviceId));
    }

    device_ids[IO_Out].clear();
    for (int i = 0; i < soundio_output_device_count(soundio); i++) device_ids[IO_Out].emplace_back(soundio_get_output_device(soundio, i)->id);

    int out_device_index = default_out_device_index;
    if (s.Audio.OutDeviceId) {
        bool found = false;
        for (int i = 0; i < soundio_output_device_count(soundio); i++) {
            auto *device = soundio_get_output_device(soundio, i);
            if (s.Audio.OutDeviceId == device->id) {
                out_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found) throw std::runtime_error(string("Invalid output device id: ") + string(s.Audio.OutDeviceId));
    }

    auto *in_device = soundio_get_input_device(soundio, in_device_index);
    if (!in_device) throw std::runtime_error("Could not get input device: out of memory");
    if (in_device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(in_device->probe_error));

    auto *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device) throw std::runtime_error("Could not get output device: out of memory");
    if (out_device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(out_device->probe_error));

    // This is from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_microphone.c#L308-L313,
    // but it fails with a mono microphone and stereo output, which is a common scenario that we'll happily handle.
//    soundio_device_sort_channel_layouts(out_device);
//    const auto *layout = soundio_best_matching_channel_layout(out_device->layouts, out_device->layout_count, in_device->layouts, in_device->layout_count);
//    if (!layout) throw std::runtime_error("Channel layouts not compatible");

    device_sample_rates.clear();
    for (int i = 0; i < in_device->sample_rate_count; i++) device_sample_rates[IO_In].push_back(in_device->sample_rates[i].max);
    if (device_sample_rates[IO_In].empty()) throw std::runtime_error("Input audio stream has no supported sample rates");
    for (int i = 0; i < out_device->sample_rate_count; i++) device_sample_rates[IO_Out].push_back(out_device->sample_rates[i].max);
    if (device_sample_rates[IO_Out].empty()) throw std::runtime_error("Output audio stream has no supported sample rates");

    instream = soundio_instream_create(in_device);
    if (!instream) throw std::runtime_error("Out of memory");
    outstream = soundio_outstream_create(out_device);
    if (!outstream) throw std::runtime_error("Out of memory");

//    instream->layout = *layout;
//    outstream->layout = *layout;

    auto prioritized_sample_rates = Audio::PrioritizedDefaultSampleRates;
    // If the project has a saved sample rate, give it the highest priority.
    if (s.Audio.SampleRate) prioritized_sample_rates.insert(prioritized_sample_rates.begin(), s.Audio.SampleRate);
    // Could just check `device_sample_rates` populated above, but this `supports_sample_rate` function handles devices supporting ranges.
    // todo support input sample rates not supported by output device
    for (const auto &preferred_sample_rate: prioritized_sample_rates) {
        if (soundio_device_supports_sample_rate(in_device, preferred_sample_rate) &&
            soundio_device_supports_sample_rate(out_device, preferred_sample_rate)) {
            instream->sample_rate = preferred_sample_rate;
            outstream->sample_rate = preferred_sample_rate;
            break;
        }
    }
    // Fall back to the highest supported sample rate. todo make sure in/out SRs match (use union)
    if (!instream->sample_rate) instream->sample_rate = device_sample_rates[IO_In].back();
    if (!outstream->sample_rate) outstream->sample_rate = device_sample_rates[IO_Out].back();
    if (outstream->sample_rate != s.Audio.SampleRate) q(set_value{s.Audio.SampleRate.Path, outstream->sample_rate});

    // todo in the libsoundio microphone example, input & output devices need the same format, since it uses a single ring buffer
    //  (I think that's why anyway!)
    //  Currently, we're handling reading & writing separately, so they don't need to be the same format.
    //  However, I want to investigate using a ring buffer as well, so keeping this as-is for now.
    for (const auto &format: prioritized_formats) {
        if (soundio_device_supports_format(in_device, format) &&
            soundio_device_supports_format(out_device, format)) {
            instream->format = format;
            outstream->format = format;
        }
    }
    if (instream->format == SoundIoFormatInvalid || outstream->format == SoundIoFormatInvalid) throw std::runtime_error("No suitable device format available");

    read_sample = read_sample_for_format(instream->format);
    write_sample = write_sample_for_format(outstream->format);

    instream->read_callback = [](SoundIoInStream *instream, int /*frame_count_min*/, int frame_count_max) {
        SoundIoChannelArea *areas;
        int err;

        last_read_frame_count_max = frame_count_max;
        int frames_left = frame_count_max;
        while (true) {
            int frame_count = frames_left;
            if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
                std::cerr << "Begin read error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }
            if (!frame_count) break;

            const auto *layout = &instream->layout;
            for (int frame = 0; frame < frame_count; frame++) {
                for (int channel = 0; channel < layout->channel_count; channel++) {
                    c.set_sample(IO_In, channel, frame, read_sample(areas[channel].ptr));
                    areas[channel].ptr += areas[channel].step;
                }
            }

            if ((err = soundio_instream_end_read(instream))) {
                if (err == SoundIoErrorUnderflow) return;
                std::cerr << "End read error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }

            frames_left -= frame_count;
            if (frames_left <= 0) break;
        }
    };

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        SoundIoChannelArea *areas;
        int err;

        last_write_frame_count_max = frame_count_max;
        int frames_left = frame_count_max;
        while (true) {
            int frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                std::cerr << "Begin write error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }
            if (!frame_count) break;
            c.compute_frames(frame_count);

            const auto *layout = &outstream->layout;
            for (int frame = 0; frame < frame_count; frame++) {
                for (int channel = 0; channel < layout->channel_count; channel++) {
                    const FAUSTFLOAT output_sample = c.get_sample(IO_Out, channel, frame) +
                        (s.Audio.MonitorInput ? c.get_sample(IO_In, min(channel, c.buffers->input_count - 1), frame) : 0);
                    write_sample(areas[channel].ptr, output_sample);
                    areas[channel].ptr += areas[channel].step;
                }
            }

            if ((err = soundio_outstream_end_write(outstream))) {
                if (err == SoundIoErrorUnderflow) return;
                std::cerr << "End write error: " << soundio_strerror(err) << std::endl;
                exit(1);
            }

            frames_left -= frame_count;
            if (frames_left <= 0) break;
        }
    };

    outstream->underflow_callback = [](SoundIoOutStream *) { std::cerr << "Underflow #" << underflow_count++ << std::endl; };

    if ((err = soundio_instream_open(instream))) { throw std::runtime_error(string("Unable to open input device: ") + soundio_strerror(err)); }
    if ((err = soundio_outstream_open(outstream))) { throw std::runtime_error(string("Unable to open output device: ") + soundio_strerror(err)); }
    if (instream->layout_error) { std::cerr << "Unable to set input channel layout: " << soundio_strerror(instream->layout_error); }
    if (outstream->layout_error) { std::cerr << "Unable to set output channel layout: " << soundio_strerror(outstream->layout_error); }
    if ((err = soundio_instream_start(instream))) { throw std::runtime_error(string("Unable to start input device: ") + soundio_strerror(err)); }
    if ((err = soundio_outstream_start(outstream))) { throw std::runtime_error(string("Unable to start output device: ") + soundio_strerror(err)); }

    soundio_ready = true;
    while (thread_running) {}

    // Cleanup
    soundio_ready = false;
    soundio_instream_destroy(instream);
    soundio_outstream_destroy(outstream);
    soundio_device_unref(in_device);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);
    soundio = nullptr;

    return 0;
}

std::thread audio_thread;

void Audio::update_process() const {
    static int previous_sample_rate = s.Audio.SampleRate;

    if (thread_running != Running) {
        thread_running = Running;
        if (Running) audio_thread = std::thread(audio);
        else audio_thread.join();
    }

    // Reset the audio thread to make any sample rate change take effect.
    if (thread_running && previous_sample_rate != s.Audio.SampleRate) {
        thread_running = false;
        audio_thread.join();
        thread_running = true;
        audio_thread = std::thread(audio);
    }
    previous_sample_rate = s.Audio.SampleRate;

    if (soundio_ready && outstream && outstream->volume != DeviceVolume) soundio_outstream_set_volume(outstream, DeviceVolume);
}

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

void PlotBuffer(const char *label, IO io, int channel, int frame_count_max) {
    const FAUSTFLOAT *buffer = c.get_samples(io, channel);
    if (buffer == nullptr) return;

    ImPlot::PlotLine(label, buffer, frame_count_max);
}

void PlotBuffers() {
    // todo use stream channel layouts & buffer size instead of hard coding
    if (ImPlot::BeginPlot("In")) {
        ImPlot::SetupAxes("Sample index", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, last_read_frame_count_max, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1);
        PlotBuffer("In (mono)", IO_In, 0, last_read_frame_count_max);
        ImPlot::EndPlot();
    }
    if (ImPlot::BeginPlot("Out")) {
        ImPlot::SetupAxes("Sample index", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, last_write_frame_count_max, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1);
        PlotBuffer("Out (left)", IO_Out, 0, last_write_frame_count_max);
        PlotBuffer("Out (right)", IO_Out, 1, last_write_frame_count_max);
        ImPlot::EndPlot();
    }
}

void Audio::draw() const {
    Running.Draw();
    Muted.Draw();
    MonitorInput.Draw();
    DeviceVolume.Draw();

    if (!device_ids[IO_In].empty()) InDeviceId.Draw(device_ids[IO_In]);
    if (!device_ids[IO_Out].empty()) OutDeviceId.Draw(device_ids[IO_Out]);
    if (!device_sample_rates[IO_Out].empty()) SampleRate.Draw(device_sample_rates[IO_Out]); // todo only show sample rates supported by both I/O
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
        PlotBuffers();
    }
}
