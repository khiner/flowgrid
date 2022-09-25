// Adapted from:
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <soundio/soundio.h>

#include "Context.h"
#include "CDSPResampler.h"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

static constexpr int FloatSize = sizeof(float);

// Used to initialize the static Faust buffer.
// This is the highest `max_frame_count` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frame sizes at the transition frame.
// If it needs bumping up, bump away!
// Note: This is _not_ the device buffer size!
// todo with all the ring buffering going on now, bring this way down, and test with lots of IO setting changes
static constexpr int MAX_EXPECTED_FRAME_COUNT = 8192;

struct Buffers {
    const int num_frames = MAX_EXPECTED_FRAME_COUNT;
    const int input_count;
    const int output_count;
    float **input;
    float **output;

    Buffers(int num_input_channels, int num_output_channels) :
        input_count(num_input_channels), output_count(num_output_channels) {
        input = new float *[num_input_channels];
        output = new float *[num_output_channels];
        for (int i = 0; i < num_input_channels; i++) { input[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
        for (int i = 0; i < num_output_channels; i++) { output[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
    }

    ~Buffers() {
        for (int i = 0; i < input_count; i++) { delete[] input[i]; }
        for (int i = 0; i < output_count; i++) { delete[] output[i]; }
        delete[] input;
        delete[] output;
    }

    // todo overload `[]` operator get/set for dimensionality `[2 (input/output)][num_channels][max_num_frames (max between input_count/output_count)]
    int channel_count(IO io) const { return io == IO_In ? input_count : io == IO_Out ? output_count : 0; }
    float **get_buffer(IO io) const { return io == IO_In ? input : io == IO_Out ? output : nullptr; }
    float *get_buffer(IO io, int channel) const {
        const auto *buffer = get_buffer(io);
        return buffer ? buffer[channel] : nullptr;
    }
    inline float get(IO io, int channel, int frame) const {
        const auto *buffer = get_buffer(io, channel);
        return buffer ? buffer[frame] : 0;
    }
    inline void set(IO io, int channel, int frame, float value) const {
        auto *buffer = get_buffer(io, channel);
        if (buffer) buffer[frame] = value;
    }
};

SoundIoFormat to_soundio_format(const Audio::IoFormat format) {
    switch (format) {
        case Audio::IoFormat_Invalid: return SoundIoFormatInvalid;
        case Audio::IoFormat_Float32NE: return SoundIoFormatFloat32NE;
        case Audio::IoFormat_Float64NE: return SoundIoFormatFloat64NE;
        case Audio::IoFormat_S32NE: return SoundIoFormatS32NE;
        case Audio::IoFormat_S16NE: return SoundIoFormatS16NE;
        default: return SoundIoFormatInvalid;
    }
}
Audio::IoFormat to_audio_format(const SoundIoFormat format) {
    switch (format) {
        case SoundIoFormatInvalid : return Audio::IoFormat_Invalid;
        case SoundIoFormatFloat32NE : return Audio::IoFormat_Float32NE;
        case SoundIoFormatFloat64NE : return Audio::IoFormat_Float64NE;
        case SoundIoFormatS32NE : return Audio::IoFormat_S32NE;
        case SoundIoFormatS16NE : return Audio::IoFormat_S16NE;
        default: return Audio::IoFormat_Invalid;
    }
}

SoundIoBackend to_soundio_backend(const AudioBackend backend) {
    switch (backend) {
        case dummy: return SoundIoBackendDummy;
        case alsa: return SoundIoBackendAlsa;
        case pulseaudio: return SoundIoBackendPulseAudio;
        case jack: return SoundIoBackendJack;
        case coreaudio: return SoundIoBackendCoreAudio;
        case wasapi: return SoundIoBackendWasapi;
        case none:
        default:cerr << "Invalid backend: " << backend << ". Defaulting to `SoundIoBackendNone`.\n";
            return SoundIoBackendNone;
    }
}

inline static float read_sample_s16ne(const char *ptr) {
    const auto value = *(int16_t *) ptr;
    return 2.0f * float(value) / (float(INT16_MAX) - float(INT16_MIN));
}
inline static float read_sample_s32ne(const char *ptr) {
    const auto value = *(int32_t *) ptr;
    return 2.0f * float(value) / (float(INT32_MAX) - float(INT32_MIN));
}
inline static float read_sample_float32ne(const char *ptr) {
    const auto value = *(float *) ptr;
    return float(value);
}
inline static float read_sample_float64ne(const char *ptr) {
    const auto value = *(double *) ptr;
    return float(value);
}

inline static void write_sample_s16ne(char *ptr, float sample) {
    auto *buf = (int16_t *) ptr;
    *buf = int16_t(sample * (float(INT16_MAX) - float(INT16_MIN)) / 2.0);
}
inline static void write_sample_s32ne(char *ptr, float sample) {
    auto *buf = (int32_t *) ptr;
    *buf = int32_t(sample * (float(INT32_MAX) - float(INT32_MIN)) / 2.0);
}
inline static void write_sample_float32ne(char *ptr, float sample) {
    auto *buf = (float *) ptr;
    *buf = float(sample);
}
inline static void write_sample_float64ne(char *ptr, float sample) {
    auto *buf = (double *) ptr;
    *buf = double(sample);
}

auto read_sample_for_format(const SoundIoFormat soundio_format) {
    switch (soundio_format) {
        case SoundIoFormatFloat32NE: return read_sample_float32ne;
        case SoundIoFormatFloat64NE: return read_sample_float64ne;
        case SoundIoFormatS32NE: return read_sample_s32ne;
        case SoundIoFormatS16NE: return read_sample_s16ne;
        default: throw std::runtime_error(format("No `read_sample` function defined for format {}", soundio_format_string(soundio_format)));
    }
}
auto write_sample_for_format(const SoundIoFormat soundio_format) {
    switch (soundio_format) {
        case SoundIoFormatFloat32NE: return write_sample_float32ne;
        case SoundIoFormatFloat64NE: return write_sample_float64ne;
        case SoundIoFormatS32NE: return write_sample_s32ne;
        case SoundIoFormatS16NE: return write_sample_s16ne;
        default: throw std::runtime_error(format("No `write_sample` function defined for format {}", soundio_format_string(soundio_format)));
    }
}

// These IO read/write functions are determined at runtime below.
static float (*read_sample)(const char *ptr);
static void (*write_sample)(char *ptr, float sample);

SoundIo *soundio = nullptr;
SoundIoInStream *instream = nullptr;
SoundIoOutStream *outstream = nullptr;
std::map<IO, std::vector<string>> device_ids = {{IO_In, {}}, {IO_Out, {}}};
std::map<IO, std::vector<Audio::IoFormat>> supported_formats = {{IO_In, {}}, {IO_Out, {}}};
std::map<IO, std::vector<int>> supported_sample_rates = {{IO_In, {}}, {IO_Out, {}}};
std::map<IO, SoundIoDevice *> devices = {{IO_In, nullptr}, {IO_Out, nullptr}};

/**
 Samples from the microphone are always read directly into `input_buffer_direct`, with no sample format/rate conversion.
 `input_buffer` will always contain 32 bit float samples, with the same sample rate as the output stream.
 Details:
 * If the input stream's sample rate matches the output stream, and it is using float32 format, `input_buffer` will simply point to `input_buffer_direct`.
 * If the input stream's sample rate matches the output stream, but it is _not_ using float32 format, `input_buffer` will reflect `input_buffer_direct`,
   but with samples converted to float32. No additional latency will be incurred in this case.
 * If the output stream sample rate is different from the input, `input_buffer` will contain the resampled (and possibly reformatted) input samples,
   with additional sample latency incurred to perform the resampling in blocks.
   todo notes about the exact latency
*/
SoundIoRingBuffer *input_buffer = nullptr, *input_buffer_direct = nullptr;
SoundIoChannelArea *in_areas = nullptr, *out_areas = nullptr;
unique_ptr<Buffers> faust_buffers;

static float mic_latency = 0.2; // Seconds todo do better than this guess
static const int MaxThreadRetries = 5; // Max number of times in a row the audio thread will try again after an error opening the IO streams

int underflow_count = 0;
int last_read_frame_count = 0, last_write_frame_count = 0;

bool soundio_ready = false, thread_running = false, first_run = true, retry_thread = false;
int retry_thread_attempt = 0;

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

    auto prioritized_formats = Audio::PrioritizedDefaultFormats;
    // If the project has a saved format, give it the highest priority.
    Enum saved_format = io == IO_In ? s.Audio.InFormat : s.Audio.OutFormat;
    if (saved_format != Audio::IoFormat_Invalid) prioritized_formats.insert(prioritized_formats.begin(), Audio::IoFormat(saved_format.value));
    auto *soundio_format_ptr = &(io == IO_In ? instream->format : outstream->format);
    for (const auto &format: prioritized_formats) {
        const auto soundio_format = to_soundio_format(format);
        if (soundio_device_supports_format(devices[io], soundio_format)) {
            *soundio_format_ptr = soundio_format;
            break;
        }
    }
    // Fall back to the highest supported format.
    for (int i = 0; i < device->format_count; i++) {
        const auto audio_format = to_audio_format(device->formats[i]);
        if (audio_format != Audio::IoFormat_Invalid) {
            supported_formats[io].push_back(audio_format);
        } else {
            cerr << "Unhandled device format: " << device->formats[i] << '\n';
        }
    }
    if (!(*soundio_format_ptr) && !supported_formats.empty()) *soundio_format_ptr = to_soundio_format(supported_formats[io].back());
    if (*soundio_format_ptr == SoundIoFormatInvalid) throw std::runtime_error(format("Audio {} device does not support any FG-supported formats", capitalize(to_string(io))));

    auto prioritized_sample_rates = Audio::PrioritizedDefaultSampleRates;
    // If the project has a saved sample rate, give it the highest priority.
    Int saved_sample_rate = io == IO_In ? s.Audio.InSampleRate : s.Audio.OutSampleRate;
    if (saved_sample_rate) prioritized_sample_rates.insert(prioritized_sample_rates.begin(), saved_sample_rate);

    // Could just check `supported_sample_rates`, but this `supports_sample_rate` function handles devices supporting ranges.
    auto *sample_rate_ptr = &(io == IO_In ? instream->sample_rate : outstream->sample_rate);
    for (const auto &preferred_sample_rate: prioritized_sample_rates) {
        if (soundio_device_supports_sample_rate(devices[io], preferred_sample_rate)) {
            *sample_rate_ptr = preferred_sample_rate;
            break;
        }
    }
    // Fall back to the highest supported sample rate.
    if (!(*sample_rate_ptr) && !supported_sample_rates.empty()) *sample_rate_ptr = supported_sample_rates[io].back();
    if (*soundio_format_ptr == SoundIoFormatInvalid) throw std::runtime_error(format("No audio {} device sample rate available", to_string(io)));
}
static void open_stream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_open(instream) : soundio_outstream_open(outstream)))) {
        throw std::runtime_error(format("Unable to open audio {} device: ", to_string(io)) + soundio_strerror(err));
    }

    if (io == IO_In && instream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(instream->layout_error); }
    else if (io == IO_Out && outstream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(outstream->layout_error); }
}
static void start_stream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_start(instream) : soundio_outstream_start(outstream)))) {
        throw std::runtime_error(format("Unable to start audio {} device: ", to_string(io)) + soundio_strerror(err));
    }
}
static void destroy_stream(const IO io) {
    if (io == IO_None) return;

    if (io == IO_In) soundio_instream_destroy(instream);
    else soundio_outstream_destroy(outstream);

    soundio_device_unref(devices[io]);
}

int audio() {
    // Local copies for bookkeeping in read/write callbacks, so others can safely use the global `in/out_areas` above (e.g. for charting):
    static char *in_area_pointers[SOUNDIO_MAX_CHANNELS], *out_area_pointers[SOUNDIO_MAX_CHANNELS];

    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = s.Audio.Backend == AudioBackend::none ? soundio_connect(soundio) : soundio_connect_backend(soundio, to_soundio_backend(s.Audio.Backend));
    if (err) throw std::runtime_error(string("Unable to connect to backend: ") + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Input/output device setup
    supported_formats.clear();
    supported_sample_rates.clear();
    for (IO io: {IO_In, IO_Out}) {
        int default_device_index = get_default_device_index(io);
        if (default_device_index < 0) throw std::runtime_error(format("No audio {} device found", to_string(io))); // todo move on without input

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
            if (!found) throw std::runtime_error(format("Invalid audio {} device id: ", to_string(io)) + string(s.Audio.get_device_id(io)));
        }

        auto *device = get_device(io, device_index);
        if (!device) throw std::runtime_error(format("Could not get audio {} device: out of memory", to_string(io)));
        if (device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(device->probe_error));

        for (int i = 0; i < device->sample_rate_count; i++) supported_sample_rates[io].push_back(device->sample_rates[i].max);
        if (supported_sample_rates[io].empty()) throw std::runtime_error(format("{} audio stream has no supported sample rates", capitalize(to_string(io))));

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

    // Input/output sample rates are settled. Set up resampler if needed.
    unique_ptr<r8b::CDSPResampler24> resampler;
    if (instream->sample_rate != outstream->sample_rate) {
        resampler = make_unique<r8b::CDSPResampler24>(instream->sample_rate, outstream->sample_rate, instream->bytes_per_frame / instream->bytes_per_sample);
    }
    read_sample = read_sample_for_format(instream->format);
    write_sample = write_sample_for_format(outstream->format);

    instream->read_callback = [](SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
        char *write_ptr_direct = soundio_ring_buffer_write_ptr(input_buffer_direct);
        const int free_count = soundio_ring_buffer_free_count(input_buffer_direct) / instream->bytes_per_frame;
        if (frame_count_min > free_count) {
            cerr << format("Ring buffer overflow: free_count:{}, frame_count_min:{}", free_count, frame_count_min);
            exit(1);
        }

        const int write_frames = min(free_count, frame_count_max);
        int frames_left = write_frames;
        int err;
        while (true) {
            int frame_count = frames_left;
            if ((err = soundio_instream_begin_read(instream, &in_areas, &frame_count))) {
                cerr << "Begin read error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            last_read_frame_count = frame_count;
            if (!frame_count) break;

            if (!in_areas) {
                // Due to an overflow there is a hole. Fill the hole in the ring buffer with silence.
                memset(write_ptr_direct, 0, frame_count * instream->bytes_per_frame * instream->layout.channel_count);
                cerr << format("Dropped {} frames due to internal overflow", frame_count) << '\n';
            } else {
                // Make a local copy of the input area pointers for incrementing, so others can read directly from the device areas.
                // todo Others assume float32, so we'll need indirect converted buffers when the input stream uses a different format.
                for (int channel = 0; channel < instream->layout.channel_count; channel += 1) in_area_pointers[channel] = in_areas[channel].ptr;

                for (int frame = 0; frame < frame_count; frame += 1) {
                    for (int channel = 0; channel < instream->layout.channel_count; channel += 1) {
                        memcpy(write_ptr_direct, in_area_pointers[channel], instream->bytes_per_sample);
                        in_area_pointers[channel] += in_areas[channel].step;
                        write_ptr_direct += instream->bytes_per_sample;
                    }
                }
            }
            soundio_ring_buffer_advance_write_ptr(input_buffer_direct, frame_count * instream->bytes_per_frame * instream->layout.channel_count);

            if ((err = soundio_instream_end_read(instream))) {
                if (err == SoundIoErrorUnderflow) return;

                cerr << "End read error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            frames_left -= frame_count;
            if (frames_left <= 0) break;
        }

        // If `input_buffer` doesn't point to `input_buffer_direct`, either the input stream's format is not float32,
        // or it has a different sample rate than the output stream.
        // todo currently only handling format changes. Handle sample rate conversion.
        if (input_buffer != input_buffer_direct) {
            auto *write_ptr = (float *) soundio_ring_buffer_write_ptr(input_buffer);
            char *read_ptr = soundio_ring_buffer_read_ptr(input_buffer_direct);
            for (int frame = 0; frame < write_frames; frame++) {
                for (int channel = 0; channel < instream->layout.channel_count; channel++) {
                    *write_ptr = read_sample(read_ptr);
                    read_ptr += instream->bytes_per_sample;
                    write_ptr += 1;
                }
            }

            soundio_ring_buffer_advance_read_ptr(input_buffer_direct, write_frames * instream->bytes_per_sample * instream->layout.channel_count);
            soundio_ring_buffer_advance_write_ptr(input_buffer, write_frames * FloatSize * instream->layout.channel_count);
        }
    };

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*frame_count_min*/, int frame_count_max) {
        // `input_sample_count` is the number of samples ready to read from the ring buffer.
        // `input_buffer` always stores float32 samples with the same sample rate as the output stream.
        const int input_sample_count = soundio_ring_buffer_fill_count(input_buffer) / FloatSize;
        const bool compute_faust = s.Audio.FaustRunning && faust_buffers && c.faust && c.faust->dsp;
        if (compute_faust) {
            if (frame_count_max > faust_buffers->num_frames) {
                cerr << "The output stream buffer only has " << faust_buffers->num_frames
                     << " frames, which is smaller than the libsoundio callback buffer size of " << frame_count_max << ".\n"
                     << "(Increase `AudioContext.MAX_EXPECTED_FRAME_COUNT`.)\n";
            }

            // If the current Faust program accepts inputs, copy any filled bytes from the device to the Faust input buffers.
            const int faust_in_channel_count = faust_buffers->channel_count(IO_In);
            if (faust_in_channel_count > 0) {
                auto *read_ptr = (float *) soundio_ring_buffer_read_ptr(input_buffer);
                for (int frame = 0; frame < input_sample_count; frame++) {
                    for (int channel = 0; channel < instream->layout.channel_count; channel++) {
                        const float value = *read_ptr;
                        if (faust_in_channel_count < channel) {
                            faust_buffers->set(IO_In, channel, frame, value);
                        } else {
                            // Sum any input channels beyond the number of Faust inputs into the last Faust input.
                            // todo handle the inverse: more Faust input channels than device inputs
                            faust_buffers->set(IO_In, faust_in_channel_count - 1, frame, faust_buffers->get(IO_In, faust_in_channel_count - 1, frame) + value);
                        }
                        read_ptr += 1;
                    }
                }
                // If the device input buffer does not have enough data filled, set the remaining Faust input samples to zero.
                for (int frame = 0; frame < frame_count_max - input_sample_count; frame++) {
                    for (int channel = 0; channel < faust_in_channel_count; channel++) {
                        faust_buffers->set(IO_In, channel, frame, 0);
                    }
                }
                // Not advancing the input read pointer until after any input monitoring below.
            }

            // Compute the Faust output buffer.
            c.faust->dsp->compute(frame_count_max, faust_buffers->input, faust_buffers->output);
        }

        int err;
        int frames_left = frame_count_max;
        while (frames_left > 0) {
            int frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &out_areas, &frame_count))) {
                cerr << "Begin write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            last_write_frame_count = frame_count;
            if (frame_count <= 0) break;

            // Make a local copy of the output area pointers for incrementing, so others can read directly from the device areas.
            // todo Others assume float32, so we'll need indirect converted buffers when the output stream uses a different format.
            for (int channel = 0; channel < outstream->layout.channel_count; channel += 1) out_area_pointers[channel] = out_areas[channel].ptr;

            auto *read_ptr = (float *) soundio_ring_buffer_read_ptr(input_buffer);
            for (int frame_inner = 0; frame_inner < frame_count; frame_inner++) {
                for (int channel = 0; channel < outstream->layout.channel_count; channel += 1) {
                    float out_sample = 0;
                    if (!s.Audio.Muted) {
                        // Monitor input directly from the ring buffer.
                        if (s.Audio.MonitorInput) out_sample += *read_ptr;
                        if (compute_faust) {
                            out_sample += faust_buffers->get(
                                IO_Out, min(channel, faust_buffers->channel_count(IO_Out) - 1),
                                frame_count_max - frames_left + frame_inner // outer frame index
                            );
                        }
                    }

                    write_sample(out_area_pointers[channel], out_sample);
                    out_area_pointers[channel] += out_areas[channel].step;
                }
                read_ptr += 1; // todo xxx this assumes mono input!
            }
            soundio_ring_buffer_advance_read_ptr(input_buffer, min(input_sample_count, frame_count) * FloatSize);

            if ((err = soundio_outstream_end_write(outstream))) {
                if (err == SoundIoErrorUnderflow) return;
                cerr << "End write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            frames_left -= frame_count;
        }
    };

    outstream->underflow_callback = [](SoundIoOutStream *) { cerr << "Underflow #" << underflow_count++ << '\n'; };

    try {
        for (IO io: {IO_In, IO_Out}) open_stream(io);
    } catch (const std::exception &e) {
        // On Mac, sometimes microphone input stream open will fail if it's already been opened and closed recently.
        // It seems sometimes it doesn't fully shut down, so starting up again can error with a
        // [1852797029 OS error](https://developer.apple.com/forums/thread/133990),
        // since it thinks the microphone is being used by another application.
        // If we run into any stream-open failure: signal to try again via `retry_thread`, clean up, and exit.
        if (++retry_thread_attempt > MaxThreadRetries) throw std::runtime_error(e.what());

        cerr << e.what() << "\nRetrying (attempt " << retry_thread_attempt << ")\n";
        retry_thread = true;
    }

    if (!retry_thread) {
        retry_thread_attempt = 0;

        // Initialize the input ring buffer(s) and Faust buffers.
        const int input_buffer_capacity_samples = ceil(mic_latency * 2 * float(instream->sample_rate));
        input_buffer_direct = soundio_ring_buffer_create(soundio, input_buffer_capacity_samples * instream->bytes_per_frame);
        if (!input_buffer_direct) throw std::runtime_error("Unable to create direct input buffer: Out of memory");

        if (instream->format == SoundIoFormatFloat32NE) {
            input_buffer = input_buffer_direct;
        } else {
            input_buffer = soundio_ring_buffer_create(soundio, input_buffer_capacity_samples * FloatSize);
            if (!input_buffer) throw std::runtime_error("Unable to create input buffer: Out of memory");
        }

        for (IO io: {IO_In, IO_Out}) start_stream(io);

        if (first_run) {
            std::map < JsonPath, json > values;
            if (instream->device->id != s.Audio.InDeviceId) values[s.Audio.InDeviceId.Path] = instream->device->id;
            if (outstream->device->id != s.Audio.OutDeviceId) values[s.Audio.OutDeviceId.Path] = outstream->device->id;
            if (instream->sample_rate != s.Audio.InSampleRate) values[s.Audio.InSampleRate.Path] = instream->sample_rate;
            if (outstream->sample_rate != s.Audio.OutSampleRate) values[s.Audio.OutSampleRate.Path] = outstream->sample_rate;
            if (instream->format != s.Audio.InFormat) values[s.Audio.InFormat.Path] = to_audio_format(instream->format);
            if (outstream->format != s.Audio.OutFormat) values[s.Audio.OutFormat.Path] = to_audio_format(outstream->format);
            if (!values.empty()) q(set_values{values});
            first_run = false;
        }

        soundio_ready = true;
        while (thread_running) {}
        soundio_ready = false;
    }

    for (IO io: {IO_In, IO_Out}) destroy_stream(io);
    soundio_destroy(soundio);
    soundio = nullptr;

    if (input_buffer != input_buffer_direct) {
        soundio_ring_buffer_destroy(input_buffer);
        input_buffer = nullptr;
    }
    soundio_ring_buffer_destroy(input_buffer_direct);
    input_buffer_direct = nullptr;

    return 0;
}

static std::thread audio_thread;

void Audio::update_process() const {
    if (thread_running != Running) {
        thread_running = Running;
        if (audio_thread.joinable()) audio_thread.join();
        if (Running) audio_thread = std::thread(audio);
    } else if (thread_running &&
        (instream->device->id != s.Audio.InDeviceId || outstream->device->id != s.Audio.OutDeviceId ||
            instream->sample_rate != s.Audio.InSampleRate || outstream->sample_rate != s.Audio.OutSampleRate ||
            instream->format != to_soundio_format(s.Audio.InFormat) || outstream->format != to_soundio_format(s.Audio.OutFormat))) {
        // Reset the audio thread to make any sample rate change take effect
        // (except the first change from 0 to a supported sample rate on startup).
        thread_running = false;
        audio_thread.join();
        thread_running = true;
        audio_thread = std::thread(audio);
    }

    if (soundio_ready && outstream && outstream->volume != OutDeviceVolume) soundio_outstream_set_volume(outstream, OutDeviceVolume);
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

void ShowBufferPlots() {
    for (IO io: {IO_In, IO_Out}) {
        const bool is_in = io == IO_In;
        if (TreeNode(capitalize(to_string(io)).c_str())) {
            const auto *area = is_in ? in_areas : out_areas;
            if (!area) continue;

            const auto *device = is_in ? instream->device : outstream->device;
            const auto &layout = is_in ? instream->layout : outstream->layout;
            const auto frame_count = is_in ? last_read_frame_count : last_write_frame_count;
            if (ImPlot::BeginPlot(device->name, {-1, 160})) {
                ImPlot::SetupAxes("Sample index", "Value");
                ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
                for (int channel_index = 0; channel_index < layout.channel_count; channel_index++) {
                    const auto &channel = layout.channels[channel_index];
                    const char *channel_name = soundio_get_channel_name(channel);
                    // todo Adapt the pointer casting to the sample format.
                    //  Also, this works but very scary and I can't even justify why this seems to work so well,
                    //  since the area pointer position gets updated in the separate read/write callbacks on the audio thread.
                    //  Hrm.. are the start points of each channel area static after initializing the stream?
                    //  If so, could just set those once on stream init and use them hear!
                    ImPlot::PlotLine(channel_name, (float *) area[channel_index].ptr, frame_count);
                }
                ImPlot::EndPlot();
            }
            TreePop();
        }
    }
}

void Audio::draw() const {
    if (!faust_buffers && c.faust && c.faust->dsp) {
        faust_buffers = make_unique<Buffers>(c.faust->dsp->getNumInputs(), c.faust->dsp->getNumOutputs());
    } else if (faust_buffers && !(c.faust && c.faust->dsp)) {
        faust_buffers = nullptr;
    }
    if (retry_thread) {
        retry_thread = false;
        thread_running = false;
        update_process();
    }
    Running.Draw();
    if (!soundio_ready) {
        Text("No audio context created yet");
        return;
    }

    FaustRunning.Draw();
    Muted.Draw();
    MonitorInput.Draw();
    OutDeviceVolume.Draw();

    if (!device_ids[IO_In].empty()) InDeviceId.Draw(device_ids[IO_In]);
    if (!device_ids[IO_Out].empty()) OutDeviceId.Draw(device_ids[IO_Out]);
    if (!supported_formats[IO_In].empty()) InFormat.Draw(supported_formats[IO_In]);
    if (!supported_formats[IO_Out].empty()) OutFormat.Draw(supported_formats[IO_Out]);
    if (!supported_sample_rates[IO_In].empty()) InSampleRate.Draw(supported_sample_rates[IO_In]);
    if (!supported_sample_rates[IO_Out].empty()) OutSampleRate.Draw(supported_sample_rates[IO_Out]);
    NewLine();
    if (TreeNode("Devices")) {
        ShowDevices();
        TreePop();
    }
    if (TreeNode("Streams")) {
        ShowStreams();
        TreePop();
    }
    const auto backend_count = soundio_backend_count(soundio);
    if (TreeNodeEx("Backends", ImGuiTreeNodeFlags_None, "Available backends (%d)", backend_count)) {
        for (int i = 0; i < backend_count; i++) {
            const auto backend = soundio_get_backend(soundio, i);
            BulletText("%s%s", soundio_backend_name(backend), backend == soundio->current_backend ? " (current)" : "");
        }
        TreePop();
    }
    if (TreeNode("Plots")) {
        ShowBufferPlots();
        TreePop();
    }
}
