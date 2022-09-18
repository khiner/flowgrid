// Adapted from:
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <soundio/soundio.h>

#include "Context.h"

// Used to initialize the static Faust buffer.
// This is the highest `max_frame_count` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frame sizes at the transition frame.
// If it needs bumping up, bump away!
// Note: This is _not_ the device buffer size!
static const int MAX_EXPECTED_FRAME_COUNT = 8192;

struct Buffers {
    const int num_frames = MAX_EXPECTED_FRAME_COUNT;
    const int input_count;
    const int output_count;
    FAUSTFLOAT **input;
    FAUSTFLOAT **output;

    Buffers(int num_input_channels, int num_output_channels) :
        input_count(num_input_channels), output_count(num_output_channels) {
        input = new FAUSTFLOAT *[num_input_channels];
        output = new FAUSTFLOAT *[num_output_channels];
        for (int i = 0; i < num_input_channels; i++) { input[i] = new FAUSTFLOAT[MAX_EXPECTED_FRAME_COUNT]; }
        for (int i = 0; i < num_output_channels; i++) { output[i] = new FAUSTFLOAT[MAX_EXPECTED_FRAME_COUNT]; }
    }

    ~Buffers() {
        for (int i = 0; i < input_count; i++) { delete[] input[i]; }
        for (int i = 0; i < output_count; i++) { delete[] output[i]; }
        delete[] input;
        delete[] output;
    }

    // todo overload `[]` operator get/set for dimensionality `[2 (input/output)][num_channels][max_num_frames (max between input_count/output_count)]
    int channel_count(IO io) const { return io == IO_In ? input_count : io == IO_Out ? output_count : 0; }
    FAUSTFLOAT **get_buffer(IO io) const { return io == IO_In ? input : io == IO_Out ? output : nullptr; }
    FAUSTFLOAT *get_buffer(IO io, int channel) const {
        const auto *buffer = get_buffer(io);
        return buffer ? buffer[channel] : nullptr;
    }
    inline FAUSTFLOAT get(IO io, int channel, int frame) const {
        const auto *buffer = get_buffer(io, channel);
        return buffer ? buffer[frame] : 0;
    }
    inline void set(IO io, int channel, int frame, FAUSTFLOAT value) const {
        auto *buffer = get_buffer(io, channel);
        if (buffer) buffer[frame] = value;
    }

    void zero(IO io) const {
        auto *buffer = get_buffer(io);
        if (!buffer) return;
        for (int channel = 0; channel < channel_count(io); channel++) {
            auto *channel_buffer = get_buffer(io, channel);
            for (int i = 0; i < num_frames; i++) channel_buffer[i] = 0;
        }
    }
    void zero() const {
        for (const IO io: {IO_In, IO_Out}) zero(io);
    }
};

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
std::map<IO, SoundIoDevice *> devices = {{IO_In, nullptr}, {IO_Out, nullptr}};
unique_ptr<Buffers> buffers;

bool soundio_ready = false;
bool thread_running = false;
int underflow_count = 0;
int last_read_frame_count_max = 0;
int last_write_frame_count_max = 0;

void compute(int frame_count) {
    if (!buffers) return;

    if (frame_count > buffers->num_frames) {
        std::cerr << "The output stream buffer only has " << buffers->num_frames
                  << " frames, which is smaller than the libsoundio callback buffer size of " << frame_count << "." << std::endl
                  << "(Increase `AudioContext.MAX_EXPECTED_FRAME_COUNT`.)" << std::endl;
        exit(1);
    }
    if (c.faust && c.faust->dsp) {
        c.faust->dsp->compute(frame_count, buffers->input, buffers->output);
    } else {
        buffers->zero(IO_Out);
    }
}

FAUSTFLOAT *get_samples(IO io, int channel) {
    if (!buffers) return nullptr;
    return io == IO_In ? buffers->input[channel] : buffers->output[channel];
}
FAUSTFLOAT get_sample(IO io, int channel, int frame) {
    return !buffers || s.Audio.Muted ? 0 : buffers->get(io, channel, frame);
}
void set_sample(IO io, int channel, int frame, FAUSTFLOAT value) {
    if (buffers) buffers->set(io, channel, frame, value);
}

static int get_device_count(const IO io) {
    switch (io) {
        case IO_In : return soundio_input_device_count(soundio);
        case IO_Out : return soundio_output_device_count(soundio);
        case IO_None : return 0;
    }
}
static SoundIoDevice *get_device(const IO io, const int index) {
    switch (io) {
        case IO_In : return soundio_get_input_device(soundio, index);
        case IO_Out : return soundio_get_output_device(soundio, index);
        case IO_None : return nullptr;
    }
}
static int get_default_device_index(const IO io) {
    switch (io) {
        case IO_In : return soundio_default_input_device_index(soundio);
        case IO_Out : return soundio_default_output_device_index(soundio);
        case IO_None : return -1;
    }
}

static void create_stream(const IO io) {
    if (io == IO_None) return;

    auto *device = devices[io];
    if (io == IO_In) instream = soundio_instream_create(device);
    else outstream = soundio_outstream_create(device);
    if ((io == IO_In && !instream) || (io == IO_Out && !outstream)) throw std::runtime_error("Out of memory");

    auto *format_ptr = &(io == IO_In ? instream->format : outstream->format);
    for (const auto &format: prioritized_formats) {
        if (soundio_device_supports_format(devices[io], format)) {
            *format_ptr = format;
            break;
        }
    }
    if (*format_ptr == SoundIoFormatInvalid) throw std::runtime_error(format("No suitable {} device format available", to_string(io)));
}
static void open_stream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_open(instream) : soundio_outstream_open(outstream)))) {
        throw std::runtime_error(format("Unable to open {} device: ", to_string(io)) + soundio_strerror(err));
    }

    if (io == IO_In && instream->layout_error) { std::cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(instream->layout_error); }
    else if (io == IO_Out && outstream->layout_error) { std::cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(outstream->layout_error); }

    if ((err = (io == IO_In ? soundio_instream_start(instream) : soundio_outstream_start(outstream)))) {
        throw std::runtime_error(format("Unable to start {} device: ", to_string(io)) + soundio_strerror(err));
    }
}
static void destroy_stream(const IO io) {
    if (io == IO_None) return;

    if (io == IO_In) soundio_instream_destroy(instream);
    else soundio_outstream_destroy(outstream);

    soundio_device_unref(devices[io]);
}

int audio() {
    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = (s.Audio.Backend == AudioBackend::none) ? soundio_connect(soundio) : soundio_connect_backend(soundio, soundio_backend(s.Audio.Backend));
    if (err) throw std::runtime_error(string("Unable to connect to backend: ") + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Input/output device setup
    device_sample_rates.clear();
    for (IO io: {IO_In, IO_Out}) {
        int default_device_index = get_default_device_index(io);
        if (default_device_index < 0) throw std::runtime_error(format("No {} device found", to_string(io))); // todo move on without input

        device_ids[io].clear();
        const auto device_count = get_device_count(io);
        for (int i = 0; i < device_count; i++) device_ids[io].emplace_back(get_device(io, i)->id);

        int device_index = default_device_index;
        if (s.Audio.get_device_id(io)) {
            bool found = false;
            for (int i = 0; i < device_count; i++) {
                auto *device = get_device(io, i);
                if (s.Audio.get_device_id(io) == device->id) {
                    device_index = i;
                    found = true;
                    soundio_device_unref(device);
                    break;
                }
                soundio_device_unref(device);
            }
            if (!found) throw std::runtime_error(format("Invalid {} device id: ", to_string(io)) + string(s.Audio.get_device_id(io)));
        }

        auto *device = get_device(io, device_index);
        if (!device) throw std::runtime_error(format("Could not get {} device: out of memory", to_string(io)));
        if (device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(device->probe_error));

        for (int i = 0; i < device->sample_rate_count; i++) device_sample_rates[io].push_back(device->sample_rates[i].max);
        if (device_sample_rates[io].empty()) throw std::runtime_error(format("{} audio stream has no supported sample rates", capitalize(to_string(io))));

        devices[io] = device;
        create_stream(io);
    }

    // This is from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_microphone.c#L308-L313,
    // but it fails with a mono microphone and stereo output, which is a common scenario that we'll happily handle.
//    soundio_device_sort_channel_layouts(out_device);
//    const auto *layout = soundio_best_matching_channel_layout(out_device->layouts, out_device->layout_count, in_device->layouts, in_device->layout_count);
//    if (!layout) throw std::runtime_error("Channel layouts not compatible");
//    instream->layout = *layout;
//    outstream->layout = *layout;

    auto prioritized_sample_rates = Audio::PrioritizedDefaultSampleRates;
    // If the project has a saved sample rate, give it the highest priority.
    if (s.Audio.SampleRate) prioritized_sample_rates.insert(prioritized_sample_rates.begin(), s.Audio.SampleRate);
    // Could just check `device_sample_rates` populated above, but this `supports_sample_rate` function handles devices supporting ranges.
    // todo support input sample rates not supported by output device
    for (const auto &preferred_sample_rate: prioritized_sample_rates) {
        if (soundio_device_supports_sample_rate(devices[IO_In], preferred_sample_rate) &&
            soundio_device_supports_sample_rate(devices[IO_Out], preferred_sample_rate)) {
            instream->sample_rate = preferred_sample_rate;
            outstream->sample_rate = preferred_sample_rate;
            break;
        }
    }
    // Fall back to the highest supported sample rate. todo make sure in/out SRs match (use union)
    if (!instream->sample_rate) instream->sample_rate = device_sample_rates[IO_In].back();
    if (!outstream->sample_rate) outstream->sample_rate = device_sample_rates[IO_Out].back();
    if (outstream->sample_rate != s.Audio.SampleRate) q(set_value{s.Audio.SampleRate.Path, outstream->sample_rate});

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
                    set_sample(IO_In, channel, frame, read_sample(areas[channel].ptr));
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

            compute(frame_count);

            const auto *layout = &outstream->layout;
            for (int frame = 0; frame < frame_count; frame++) {
                for (int channel = 0; channel < layout->channel_count; channel++) {
                    const FAUSTFLOAT output_sample = get_sample(IO_Out, channel, frame) +
                        (s.Audio.MonitorInput ? get_sample(IO_In, min(channel, buffers->channel_count(IO_In) - 1), frame) : 0);
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

    for (IO io: {IO_In, IO_Out}) open_stream(io);
    buffers = std::make_unique<Buffers>(instream->layout.channel_count, outstream->layout.channel_count);

    soundio_ready = true;
    while (thread_running) {}
    soundio_ready = false;

    buffers = nullptr;
    for (IO io: {IO_In, IO_Out}) destroy_stream(io);
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
    for (int i = 0; i < layout.channel_count; i++) BulletText("%s", soundio_get_channel_name(layout.channels[i]));
}

void ShowDevice(const SoundIoDevice &device, bool is_default) {
    const char *default_str = is_default ? " (default)" : "";
    const char *raw_str = device.is_raw ? " (raw)" : "";
    if (TreeNode(device.name, "%s%s%s", device.name, default_str, raw_str)) {
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
    for (const IO io: {IO_In, IO_Out}) {
        const auto device_count = get_device_count(io);
        const string &io_label = capitalize(to_string(io));
        if (TreeNodeEx(format("{} devices ({})", io_label, device_count).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto default_device_index = get_default_device_index(io);
            for (int device_index = 0; device_index < device_count; device_index++) {
                auto *device = get_device(io, device_index);
                ShowDevice(*device, default_device_index == device_index);
                soundio_device_unref(device);
            }
            TreePop();
        }
    }
}

void ShowStreams() {
    // todo maybe modify SoundIoInStream/SoundIoOutStream to share a common interface to DRY up? This is a pain elsewhere, too.
    if (TreeNode("Input stream")) {
        BulletText("Name: %s", instream->name);
        BulletText("Device ID: %s", instream->device->id);
        BulletText("Format: %s", soundio_format_string(instream->format));
        BulletText("Sample rate: %d", instream->sample_rate);
        if (TreeNodeEx("Channel layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ShowChannelLayout(instream->layout, false);
            TreePop();
        }
        BulletText("Software latency: %0.8f sec", instream->software_latency);
        BulletText("Bytes per frame: %d", instream->bytes_per_frame);
        BulletText("Bytes per sample: %d", instream->bytes_per_sample);

        TreePop();
    }
    if (TreeNode("Output stream")) {
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
    const FAUSTFLOAT *buffer = get_samples(io, channel);
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
        PlotBuffer("Left", IO_Out, 0, last_write_frame_count_max);
        PlotBuffer("Right", IO_Out, 1, last_write_frame_count_max);
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
