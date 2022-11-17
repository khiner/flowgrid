// Adapted from:
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <thread> // For sleep fn
#include <soundio/soundio.h>

#include "CDSPResampler.h"
#include "Helper/Sample.h" // Must be included before any Faust includes
#include "faust/dsp/llvm-dsp.h"

#include "App.h"
#include "Helper/File.h"
#include "UI/Faust/FaustUI.h"

static constexpr int SampleSize = sizeof(Sample);

// Used to initialize the static Faust buffer.
// This is the highest `max_frames` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frames at the transition.
// If it needs bumping up, bump away!
static constexpr int FaustBufferFrames = 2048;

SoundIoFormat ToSoundIoFormat(const Audio::IoFormat format) {
    switch (format) {
        case Audio::IoFormat_Invalid: return SoundIoFormatInvalid;
        case Audio::IoFormat_Float64NE: return SoundIoFormatFloat64NE;
        case Audio::IoFormat_Float32NE: return SoundIoFormatFloat32NE;
        case Audio::IoFormat_S32NE: return SoundIoFormatS32NE;
        case Audio::IoFormat_S16NE: return SoundIoFormatS16NE;
        default: return SoundIoFormatInvalid;
    }
}
Audio::IoFormat ToAudioFormat(const SoundIoFormat format) {
    switch (format) {
        case SoundIoFormatInvalid : return Audio::IoFormat_Invalid;
        case SoundIoFormatFloat64NE : return Audio::IoFormat_Float64NE;
        case SoundIoFormatFloat32NE : return Audio::IoFormat_Float32NE;
        case SoundIoFormatS32NE : return Audio::IoFormat_S32NE;
        case SoundIoFormatS16NE : return Audio::IoFormat_S16NE;
        default: return Audio::IoFormat_Invalid;
    }
}

SoundIoBackend ToSoundIoBackend(const AudioBackend backend) {
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

inline static Sample ReadSampleFloat64NE(const char *ptr) {
    const auto value = *(double *) ptr;
    return Sample(value);
}
inline static Sample ReadSampleFloat32NE(const char *ptr) {
    const auto value = *(float *) ptr;
    return Sample(value);
}
inline static Sample ReadSampleS32NE(const char *ptr) {
    const auto value = *(int32_t *) ptr;
    return 2 * Sample(value) / (Sample(INT32_MAX) - Sample(INT32_MIN));
}
inline static Sample ReadSampleS16NE(const char *ptr) {
    const auto value = *(int16_t *) ptr;
    return 2 * Sample(value) / (Sample(INT16_MAX) - Sample(INT16_MIN));
}

inline static void WriteSampleFloat64NE(char *ptr, Sample sample) {
    auto *buf = (double *) ptr;
    *buf = double(sample);
}
inline static void WriteSampleFloat32NE(char *ptr, Sample sample) {
    auto *buf = (float *) ptr;
    *buf = float(sample);
}
inline static void WriteSampleS32NE(char *ptr, Sample sample) {
    auto *buf = (int32_t *) ptr;
    *buf = int32_t(sample * (Sample(INT32_MAX) - Sample(INT32_MIN)) / 2.0);
}
inline static void WriteSampleS16NE(char *ptr, Sample sample) {
    auto *buf = (int16_t *) ptr;
    *buf = int16_t(sample * (Sample(INT16_MAX) - Sample(INT16_MIN)) / 2.0);
}

auto ReadSampeForFormat(const SoundIoFormat soundio_format) {
    switch (soundio_format) {
        case SoundIoFormatFloat64NE: return ReadSampleFloat64NE;
        case SoundIoFormatFloat32NE: return ReadSampleFloat32NE;
        case SoundIoFormatS32NE: return ReadSampleS32NE;
        case SoundIoFormatS16NE: return ReadSampleS16NE;
        default: throw std::runtime_error(format("No `ReadSample` function defined for format {}", soundio_format_string(soundio_format)));
    }
}
auto WriteSampleForFormat(const SoundIoFormat soundio_format) {
    switch (soundio_format) {
        case SoundIoFormatFloat64NE: return WriteSampleFloat64NE;
        case SoundIoFormatFloat32NE: return WriteSampleFloat32NE;
        case SoundIoFormatS32NE: return WriteSampleS32NE;
        case SoundIoFormatS16NE: return WriteSampleS16NE;
        default: throw std::runtime_error(format("No `WriteSample` function defined for format {}", soundio_format_string(soundio_format)));
    }
}

// These IO read/write functions are determined at runtime below.
static Sample (*ReadSample)(const char *ptr);
static void (*WriteSample)(char *ptr, Sample sample);

SoundIo *soundio = nullptr;
SoundIoInStream *instream = nullptr;
SoundIoOutStream *outstream = nullptr;

// Each of these arrays can be indexed by an `IO`, e.g. `device_ids[IO_In]`.
vector<string> device_ids[IO_Count];
vector<Audio::IoFormat> supported_formats[IO_Count];
vector<int> supported_sample_rates[IO_Count];
SoundIoDevice *devices[IO_Count];
SoundIoChannelArea *areas[IO_Count];
Sample **faust_buffers[IO_Count];

/**
 Samples from the input (e.g. microphone) are read directly into `InputBufferDirect`,
 optionally performing sample format conversion, but with no sample _rate_ conversion.
 * `InputBufferDirect` will contain 64 bit double samples, with the same sample rate as the input stream.
 * `InputBuffer` will contain 64 bit double samples, with the same sample rate as the output stream.
 * If the input/output stream sample rates are the same, `InputBuffer` will simply point to `InputBufferDirect`
   todo notes about the exact latency (see https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation)
*/
SoundIoRingBuffer *InputBuffer = nullptr, *InputBufferDirect = nullptr;
unique_ptr<r8b::CDSPResampler24> Resampler;

// Faust vars
Box box = nullptr;
llvm_dsp_factory *DspFactory;
dsp *dsp = nullptr;
unique_ptr<FaustUI> faust_ui;

static float MicLatency = 0.2; // Seconds todo do better than this guess
int UnderflowCount = 0, LastReadFrameCount = 0, LastWriteFrameCount = 0;
bool SoundIoReady = false, FaustReady = false;

static int GetDeviceCount(const IO io) {
    switch (io) {
        case IO_In : return soundio_input_device_count(soundio);
        case IO_Out : return soundio_output_device_count(soundio);
        case IO_None : return 0;
    }
}
static SoundIoDevice *GetDevice(const IO io, const int index) {
    switch (io) {
        case IO_In : return soundio_get_input_device(soundio, index);
        case IO_Out : return soundio_get_output_device(soundio, index);
        case IO_None : return nullptr;
    }
}
static int GetDefaultDeviceIndex(const IO io) {
    switch (io) {
        case IO_In : return soundio_default_input_device_index(soundio);
        case IO_Out : return soundio_default_output_device_index(soundio);
        case IO_None : return -1;
    }
}
static int GetChannelCount(const IO io) {
    switch (io) {
        case IO_In : return instream ? instream->layout.channel_count : 0;
        case IO_Out : return outstream ? outstream->layout.channel_count : 0;
        case IO_None : return 0;
    }
}

static void CreateStream(const IO io) {
    if (io == IO_None) return;

    auto *device = devices[io];
    if (io == IO_In) instream = soundio_instream_create(device);
    else outstream = soundio_outstream_create(device);
    if ((io == IO_In && !instream) || (io == IO_Out && !outstream)) throw std::runtime_error("Out of memory");

    auto prioritized_formats = Audio::PrioritizedDefaultFormats;
    for (const auto &format: prioritized_formats) {
        if (format != Audio::IoFormat_Invalid && soundio_device_supports_format(device, ToSoundIoFormat(format))) {
            supported_formats[io].emplace_back(format);
        }
    }
    if (supported_formats[io].empty()) throw std::runtime_error(format("Audio {} device does not support any FG-supported formats", capitalize(to_string(io))));

    const Enum &saved_format = io == IO_In ? s.Audio.InFormat : s.Audio.OutFormat;
    // If the project has a saved format, choose it. Otherwise, default to the highest-priority supported format.
    auto *soundio_format_ptr = &(io == IO_In ? instream->format : outstream->format);
    *soundio_format_ptr = ToSoundIoFormat(saved_format != Audio::IoFormat_Invalid ? saved_format : supported_formats[io].front());

    auto prioritized_sample_rates = Audio::PrioritizedDefaultSampleRates;
    // If the project has a saved sample rate, give it the highest priority.
    const Int &saved_sample_rate = io == IO_In ? s.Audio.InSampleRate : s.Audio.OutSampleRate;
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
    if (!(*sample_rate_ptr) && !supported_sample_rates[io].empty()) *sample_rate_ptr = supported_sample_rates[io].back();
    if (*soundio_format_ptr == SoundIoFormatInvalid) throw std::runtime_error(format("No audio {} device sample rate available", to_string(io)));
}
static void OpenStream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_open(instream) : soundio_outstream_open(outstream)))) {
        throw std::runtime_error(format("Unable to open audio {} device: ", to_string(io)) + soundio_strerror(err));
    }

    if (io == IO_In && instream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(instream->layout_error); }
    else if (io == IO_Out && outstream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(outstream->layout_error); }
}
static void StartStream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_start(instream) : soundio_outstream_start(outstream)))) {
        throw std::runtime_error(format("Unable to start audio {} device: ", to_string(io)) + soundio_strerror(err));
    }
}
static void DestroyStream(const IO io) {
    if (io == IO_None) return;

    if (io == IO_In) soundio_instream_destroy(instream);
    else soundio_outstream_destroy(outstream);

    soundio_device_unref(devices[io]);
}

void SetupAudio() {
    // Local copies for bookkeeping in read/write callbacks, so others can safely use the global `areas` pointers above (e.g. for charting):
    static char *area_pointers[2][SOUNDIO_MAX_CHANNELS];

    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = s.Audio.Backend == AudioBackend::none ? soundio_connect(soundio) : soundio_connect_backend(soundio, ToSoundIoBackend(s.Audio.Backend));
    if (err) throw std::runtime_error(string("Unable to connect to backend: ") + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Input/output device setup
    for (const IO io: IO_All) {
        device_ids[io].clear();
        supported_formats[io].clear();
        supported_sample_rates[io].clear();

        int default_device_index = GetDefaultDeviceIndex(io);
        if (default_device_index < 0) throw std::runtime_error(format("No audio {} device found", to_string(io))); // todo move on without input

        const auto device_count = GetDeviceCount(io);
        for (int i = 0; i < device_count; i++) device_ids[io].emplace_back(GetDevice(io, i)->id);

        int device_index = default_device_index;
        if (s.Audio.GetDeviceId(io)) {
            bool found = false;
            for (int i = 0; i < device_count; i++) {
                auto *device = GetDevice(io, i);
                if (s.Audio.GetDeviceId(io) == device->id) {
                    device_index = i;
                    found = true;
                    soundio_device_unref(device);
                    break;
                }
                soundio_device_unref(device);
            }
            if (!found) throw std::runtime_error(format("Invalid audio {} device id: ", to_string(io)) + string(s.Audio.GetDeviceId(io)));
        }

        auto *device = GetDevice(io, device_index);
        if (!device) throw std::runtime_error(format("Could not get audio {} device: out of memory", to_string(io)));
        if (device->probe_error) throw std::runtime_error(string("Cannot probe device: ") + soundio_strerror(device->probe_error));

        for (int i = 0; i < device->sample_rate_count; i++) supported_sample_rates[io].push_back(device->sample_rates[i].max);
        if (supported_sample_rates[io].empty()) throw std::runtime_error(format("{} audio stream has no supported sample rates", capitalize(to_string(io))));

        devices[io] = device;
        CreateStream(io);
    }

    // This is from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_microphone.c#L308-L313,
    // but it fails with a mono microphone and stereo output, which is a common scenario that we'll happily handle.
//    soundio_device_sort_channel_layouts(out_device);
//    const auto *layout = soundio_best_matching_channel_layout(out_device->layouts, out_device->layout_count, in_device->layouts, in_device->layout_count);
//    if (!layout) throw std::runtime_error("Channel layouts not compatible");
//    instream->layout = *layout;
//    outstream->layout = *layout;

    instream->read_callback = [](SoundIoInStream *instream, int min_frames, int max_frames) {
        const auto channel_count = instream->layout.channel_count;
        auto *write_ptr_direct = (Sample *) soundio_ring_buffer_write_ptr(InputBufferDirect);
        const int available_write_frames = soundio_ring_buffer_free_count(InputBufferDirect) / SampleSize;
        if (min_frames > available_write_frames) {
            cerr << format("Direct input ring buffer overflow: Available:{}, Need:{}", available_write_frames, min_frames);
            exit(1);
        }

        const int write_frames = min(available_write_frames, max_frames);
        int remaining_frames = write_frames;
        int err;
        while (true) {
            int inner_frames = remaining_frames;
            if ((err = soundio_instream_begin_read(instream, &areas[IO_In], &inner_frames))) {
                cerr << "Begin read error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            LastReadFrameCount = inner_frames;
            if (!inner_frames) break;

            if (!areas[IO_In]) {
                // Due to an overflow there is a hole. Fill the hole in the ring buffer with silence.
                memset(write_ptr_direct, 0, inner_frames * SampleSize * channel_count);
                cerr << format("Dropped {} frames due to internal overflow", inner_frames) << '\n';
            } else {
                // Make a local copy of the input area pointers for incrementing, so others can read directly from the device areas.
                // todo Others assume doubles, so we'll need indirect converted buffers when the input stream uses a different format.
                for (int channel = 0; channel < channel_count; channel += 1) area_pointers[IO_In][channel] = areas[IO_In][channel].ptr;

                for (int frame = 0; frame < inner_frames; frame += 1) {
                    for (int channel = 0; channel < channel_count; channel += 1) {
                        *write_ptr_direct = ReadSample(area_pointers[IO_In][channel]);
                        area_pointers[IO_In][channel] += areas[IO_In][channel].step;
                        write_ptr_direct += 1;
                    }
                }
            }
            soundio_ring_buffer_advance_write_ptr(InputBufferDirect, inner_frames * SampleSize * channel_count);

            if ((err = soundio_instream_end_read(instream))) {
                if (err == SoundIoErrorUnderflow) return;

                cerr << "End read error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            remaining_frames -= inner_frames;
            if (remaining_frames <= 0) break;
        }

        // If `InputBuffer` doesn't point to `InputBufferDirect`, it has a different sample rate than the output stream.
        if (InputBuffer != InputBufferDirect && Resampler) {
            const int available_resample_read_frames = soundio_ring_buffer_fill_count(InputBufferDirect) / SampleSize;
            const int available_resample_write_frames = soundio_ring_buffer_free_count(InputBuffer) / SampleSize;
            auto *read_ptr = (Sample *) soundio_ring_buffer_read_ptr(InputBufferDirect);
            // todo handle multichannel input
            Sample *resampled_buffer; // Owned by resampler
            int resampled_frames = Resampler->process(read_ptr, available_resample_read_frames, resampled_buffer);
            if (resampled_frames > available_write_frames) {
                cerr << format("Resampled input ring buffer overflow: Available:{}, Need:{}", available_resample_write_frames, resampled_frames);
                exit(1);
            }
            const int resample_input_bytes = available_resample_read_frames * SampleSize * instream->layout.channel_count;
            soundio_ring_buffer_advance_read_ptr(InputBufferDirect, resample_input_bytes);

            if (resampled_frames > 0) {
                const int output_bytes = resampled_frames * SampleSize * channel_count;
                auto *write_ptr = (Sample *) soundio_ring_buffer_write_ptr(InputBuffer);
                memcpy(write_ptr, resampled_buffer, output_bytes);
                soundio_ring_buffer_advance_write_ptr(InputBuffer, output_bytes);
            }
        }
    };

    outstream->write_callback = [](SoundIoOutStream *outstream, int /*min_frames*/, int max_frames) {
        const auto channel_count = outstream->layout.channel_count;
        const int input_sample_count = soundio_ring_buffer_fill_count(InputBuffer) / SampleSize;
        const bool compute_faust = s.Audio.FaustRunning && FaustReady;

        int err;
        int remaining_frames = max_frames;
        while (remaining_frames > 0) {
            int inner_frames = remaining_frames;
            if ((err = soundio_outstream_begin_write(outstream, &areas[IO_Out], &inner_frames))) {
                cerr << "Begin write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            if (compute_faust) {
                if (inner_frames > FaustBufferFrames) {
                    cerr << "The Faust output buffer only has " << FaustBufferFrames << " frames, which is less than the required " << inner_frames << ".\n"
                         << "(Increase `Audio.cpp::FaustBufferFrames`.)\n";
                }

                if (dsp->getNumInputs() > 0 && GetChannelCount(IO_In) == 1) {
                    auto *read_ptr = (Sample *) soundio_ring_buffer_read_ptr(InputBuffer);
                    // Point every Faust input channel to the first audio input channel.
                    // Not advancing the input read pointer until after any input monitoring below.
                    // todo this works for 1:1 channel case, but need to properly handle input routing between more channels on either side
                    for (int i = 0; i < dsp->getNumInputs(); i++) faust_buffers[IO_In][i] = read_ptr;
                }

                if (FaustReady) dsp->compute(inner_frames, faust_buffers[IO_In], faust_buffers[IO_Out]);
            }

            LastWriteFrameCount = inner_frames;
            if (inner_frames <= 0) break;

            // Make a local copy of the output area pointers for incrementing, so others can read directly from the device areas.
            // todo Others assume doubles, so we'll need indirect converted buffers when the output stream uses a different format.
            for (int channel = 0; channel < channel_count; channel += 1) area_pointers[IO_Out][channel] = areas[IO_Out][channel].ptr;

            auto *read_ptr = (Sample *) soundio_ring_buffer_read_ptr(InputBuffer);
            for (int inner_frame = 0; inner_frame < inner_frames; inner_frame++) {
                for (int channel = 0; channel < channel_count; channel += 1) {
                    Sample out_sample = 0;
                    if (!s.Audio.Muted) {
                        // Monitor input directly from the ring buffer.
                        if (s.Audio.MonitorInput) out_sample += *read_ptr;
                        if (compute_faust) {
                            const int outer_frame = max_frames - remaining_frames + inner_frame;
                            out_sample += faust_buffers[IO_Out][min(channel, dsp->getNumOutputs() - 1)][outer_frame];
                        }
                    }

                    WriteSample(area_pointers[IO_Out][channel], out_sample);
                    area_pointers[IO_Out][channel] += areas[IO_Out][channel].step;
                }
                read_ptr += 1; // todo xxx this assumes mono input!
            }
            soundio_ring_buffer_advance_read_ptr(InputBuffer, min(input_sample_count, inner_frames) * SampleSize);

            if ((err = soundio_outstream_end_write(outstream))) {
                if (err == SoundIoErrorUnderflow) return;
                cerr << "End write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            remaining_frames -= inner_frames;
        }
    };

    outstream->underflow_callback = [](SoundIoOutStream *) { cerr << "Underflow #" << UnderflowCount++ << '\n'; };

    for (IO io: IO_All) OpenStream(io);

    // Set up resampler if needed.
    if (instream->sample_rate != outstream->sample_rate) {
        Resampler = make_unique<r8b::CDSPResampler24>(instream->sample_rate, outstream->sample_rate, 1024); // todo can we get max frame size here?
    }

    // Initialize the input ring buffer(s).
    InputBufferDirect = soundio_ring_buffer_create(soundio, int(ceil(MicLatency * 2 * float(instream->sample_rate)) * SampleSize));
    if (!InputBufferDirect) throw std::runtime_error("Unable to create direct input buffer: Out of memory");

    if (instream->sample_rate == outstream->sample_rate) {
        InputBuffer = InputBufferDirect;
    } else {
        InputBuffer = soundio_ring_buffer_create(soundio, int(ceil(MicLatency * 2 * float(outstream->sample_rate)) * SampleSize));
        if (!InputBuffer) throw std::runtime_error("Unable to create input buffer: Out of memory");
    }

    ReadSample = ReadSampeForFormat(instream->format);
    WriteSample = WriteSampleForFormat(outstream->format);

    for (IO io: IO_All) StartStream(io);
    SoundIoReady = true;
}

void TeardownAudio(bool startup_failed = false) {
    SoundIoReady = false;
    for (IO io: IO_All) DestroyStream(io);
    soundio_destroy(soundio);
    soundio = nullptr;

    if (!startup_failed) {
        if (InputBuffer != InputBufferDirect) {
            soundio_ring_buffer_destroy(InputBuffer);
            InputBuffer = nullptr;
        }
        soundio_ring_buffer_destroy(InputBufferDirect);
        InputBufferDirect = nullptr;
    }
}

void RetryingSetupAudio() {
    static const int MaxRetries = 5;
    static int retry_attempt = 0;

    try {
        SetupAudio();
        retry_attempt = 0;
    } catch (const std::exception &e) {
        // On Mac, sometimes microphone input stream open will fail if it's already been opened and closed recently.
        // It seems sometimes it doesn't fully shut down, so starting up again can error with a
        // [1852797029 OS error](https://developer.apple.com/forums/thread/133990),
        // since it thinks the microphone is being used by another application.
        if (++retry_attempt > MaxRetries) throw std::runtime_error(e.what());

        cerr << e.what() << "\nRetrying (attempt " << retry_attempt << ")\n";
        TeardownAudio(true);
        std::this_thread::sleep_for(100ms * (1 << (retry_attempt - 1))); // Start at 100ms and double after each additional retry.
        RetryingSetupAudio();
    }
}

string previous_faust_code;
int previous_faust_sample_rate = 0;

void Audio::UpdateProcess() const {
    if (Running && !soundio) {
        RetryingSetupAudio();
    } else if (!Running && soundio) {
        TeardownAudio();
    } else if (SoundIoReady &&
        (instream->device->id != InDeviceId || outstream->device->id != OutDeviceId ||
            instream->sample_rate != s.Audio.InSampleRate || outstream->sample_rate != OutSampleRate ||
            instream->format != ToSoundIoFormat(InFormat) || outstream->format != ToSoundIoFormat(OutFormat))) {
        // Reset to make any audio config changes take effect.
        TeardownAudio();
        RetryingSetupAudio();
    }

    static bool first_run = true;
    if (first_run) {
        first_run = false;

        static StoreEntries values;
        if (instream->device->id != InDeviceId) values.emplace_back(InDeviceId.Path, instream->device->id);
        if (outstream->device->id != OutDeviceId) values.emplace_back(OutDeviceId.Path, outstream->device->id);
        if (instream->sample_rate != InSampleRate) values.emplace_back(InSampleRate.Path, instream->sample_rate);
        if (outstream->sample_rate != OutSampleRate) values.emplace_back(OutSampleRate.Path, outstream->sample_rate);
        if (instream->format != InFormat) values.emplace_back(InFormat.Path, ToAudioFormat(instream->format));
        if (outstream->format != OutFormat) values.emplace_back(OutFormat.Path, ToAudioFormat(outstream->format));
        if (!values.empty()) q(set_values{values}, true);
    }

    if (Faust.Code != previous_faust_code || OutSampleRate != previous_faust_sample_rate) {
        previous_faust_code = string(Faust.Code);
        previous_faust_sample_rate = OutSampleRate;

        string error_msg;

        destroyLibContext();
        if (Faust.Code && OutSampleRate) {
            createLibContext();

            int argc = 0;
            const char **argv = new const char *[8];
            argv[argc++] = "-I";
            argv[argc++] = fs::relative("../lib/faust/libraries").c_str();
            argv[argc++] = "-double";

            int num_inputs, num_outputs;
            box = DSPToBoxes("FlowGrid", Faust.Code, argc, argv, &num_inputs, &num_outputs, error_msg);
            if (box && error_msg.empty()) {
                static const int optimize_level = -1;
                DspFactory = createDSPFactoryFromBoxes("FlowGrid", box, argc, argv, "", error_msg, optimize_level);
            }
        }
        if (DspFactory && error_msg.empty()) {
            dsp = DspFactory->createDSPInstance();
            dsp->init(OutSampleRate);

            // Init `faust_buffers`
            for (const IO io: IO_All) {
                const int channels = io == IO_In ? dsp->getNumInputs() : dsp->getNumOutputs();
                if (channels > 0) faust_buffers[io] = new Sample *[channels];
            }
            for (int i = 0; i < dsp->getNumOutputs(); i++) { faust_buffers[IO_Out][i] = new Sample[FaustBufferFrames]; }

            FaustReady = true;
            faust_ui = make_unique<FaustUI>();
            dsp->buildUserInterface(faust_ui.get());
        } else {
            faust_ui = nullptr;
            FaustReady = false;

            if (dsp) {
                // Destroy `faust_buffers`
                for (int i = 0; i < dsp->getNumOutputs(); i++) { delete[] faust_buffers[IO_Out][i]; }
                for (const IO io: IO_All) {
                    delete[] faust_buffers[io];
                    faust_buffers[io] = nullptr;
                }
                delete dsp;
                dsp = nullptr;
                deleteDSPFactory(DspFactory);
                DspFactory = nullptr;
            }
        }

        if (!error_msg.empty()) q(set_value{Faust.Error.Path, error_msg});
        else if (Faust.Error) q(set_value{Faust.Error.Path, ""});

        OnBoxChange(box);
        OnUiChange(faust_ui.get());
    }

    if (SoundIoReady && outstream->volume != OutDeviceVolume) soundio_outstream_set_volume(outstream, OutDeviceVolume);
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
    for (const IO io: IO_All) {
        const auto device_count = GetDeviceCount(io);
        const string &io_label = capitalize(to_string(io));
        if (TreeNodeEx(format("{} devices ({})", io_label, device_count).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto default_device_index = GetDefaultDeviceIndex(io);
            for (int device_index = 0; device_index < device_count; device_index++) {
                auto *device = GetDevice(io, device_index);
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
    for (IO io: IO_All) {
        const bool is_in = io == IO_In;
        if (TreeNode(capitalize(to_string(io)).c_str())) {
            const auto *area = is_in ? areas[IO_In] : areas[IO_Out];
            if (!area) continue;

            const auto *device = is_in ? instream->device : outstream->device;
            const auto &layout = is_in ? instream->layout : outstream->layout;
            const auto frame_count = is_in ? LastReadFrameCount : LastWriteFrameCount;
            if (ImPlot::BeginPlot(device->name, {-1, 160})) {
                ImPlot::SetupAxes("Sample index", "Value");
                ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
                for (int channel_index = 0; channel_index < layout.channel_count; channel_index++) {
                    const auto &channel = layout.channels[channel_index];
                    const char *channel_name = soundio_get_channel_name(channel);
                    // todo Adapt the pointer casting to the sample format.
                    //  Also, this works but very scary and I can't even justify why this seems to work so well,
                    //  since the area pointer position gets updated in the separate read/write callbacks.
                    //  Hrm.. are the start points of each channel area static after initializing the stream?
                    //  If so, could just set those once on stream init and use them here!
                    ImPlot::PlotLine(channel_name, (Sample *) area[channel_index].ptr, frame_count);
                }
                ImPlot::EndPlot();
            }
            TreePop();
        }
    }
}

void Audio::Draw() const {
    Running.Draw();
    if (!SoundIoReady) {
        TextUnformatted("No audio context created yet");
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
