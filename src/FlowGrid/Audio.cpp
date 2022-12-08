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
        default: {
            cerr << "Invalid backend: " << backend << ". Defaulting to `SoundIoBackendNone`.\n";
            return SoundIoBackendNone;
        }
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

auto ReadSampleForFormat(const SoundIoFormat soundio_format) {
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
SoundIoInStream *InStream = nullptr;
SoundIoOutStream *OutStream = nullptr;

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
        case IO_In : return InStream ? InStream->layout.channel_count : 0;
        case IO_Out : return OutStream ? OutStream->layout.channel_count : 0;
        case IO_None : return 0;
    }
}

/**
 Samples from the input (e.g. microphone) are read directly into `InputBufferDirect`,
 optionally performing sample format conversion, but with no sample _rate_ conversion.
 * `InputBufferDirect` will contain 64 bit double samples, with the same sample rate as the input stream.
 * `InputBuffer` will contain 64 bit double samples, with the same sample rate as the output stream.
 * If the input/output stream sample rates are the same, `InputBuffer` will simply point to `InputBufferDirect`
   todo notes about the exact latency (see https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation)
*/
static SoundIoRingBuffer *InputBuffer = nullptr, *InputBufferDirect = nullptr;
static unique_ptr<r8b::CDSPResampler24> Resampler;

// Each of these arrays can be indexed by an `IO`, e.g. `DeviceIds[IO_In]`.
static vector<string> DeviceIds[IO_Count];
static vector<Audio::IoFormat> SupportedFormats[IO_Count];
static vector<int> SupportedSampleRates[IO_Count];
static SoundIoDevice *Devices[IO_Count];
static SoundIoChannelArea *Areas[IO_Count];
static Sample **FaustBuffers[IO_Count];

static void CreateStream(const IO io) {
    if (io == IO_None) return;

    auto *device = Devices[io];
    if (io == IO_In) InStream = soundio_instream_create(device);
    else OutStream = soundio_outstream_create(device);
    if ((io == IO_In && !InStream) || (io == IO_Out && !OutStream)) throw std::runtime_error("Out of memory");

    auto prioritized_formats = Audio::PrioritizedDefaultFormats;
    for (const auto &format: prioritized_formats) {
        if (format != Audio::IoFormat_Invalid && soundio_device_supports_format(device, ToSoundIoFormat(format))) {
            SupportedFormats[io].emplace_back(format);
        }
    }
    if (SupportedFormats[io].empty()) throw std::runtime_error(format("Audio {} device does not support any FG-supported formats", Capitalize(to_string(io))));

    const Enum &saved_format = io == IO_In ? s.Audio.InFormat : s.Audio.OutFormat;
    // If the project has a saved format, choose it. Otherwise, default to the highest-priority supported format.
    auto *soundio_format_ptr = &(io == IO_In ? InStream->format : OutStream->format);
    *soundio_format_ptr = ToSoundIoFormat(saved_format != Audio::IoFormat_Invalid ? saved_format : SupportedFormats[io].front());

    auto prioritized_sample_rates = Audio::PrioritizedDefaultSampleRates;
    // If the project has a saved sample rate, give it the highest priority.
    const Int &saved_sample_rate = io == IO_In ? s.Audio.InSampleRate : s.Audio.OutSampleRate;
    if (saved_sample_rate) prioritized_sample_rates.insert(prioritized_sample_rates.begin(), saved_sample_rate);

    // Could just check `SupportedSampleRates`, but this `supports_sample_rate` function handles devices supporting ranges.
    auto *sample_rate_ptr = &(io == IO_In ? InStream->sample_rate : OutStream->sample_rate);
    for (const auto &preferred_sample_rate: prioritized_sample_rates) {
        if (soundio_device_supports_sample_rate(Devices[io], preferred_sample_rate)) {
            *sample_rate_ptr = preferred_sample_rate;
            break;
        }
    }
    // Fall back to the highest supported sample rate.
    if (!(*sample_rate_ptr) && !SupportedSampleRates[io].empty()) *sample_rate_ptr = SupportedSampleRates[io].back();
    if (*soundio_format_ptr == SoundIoFormatInvalid) throw std::runtime_error(format("No audio {} device sample rate available", to_string(io)));
}
static void OpenStream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_open(InStream) : soundio_outstream_open(OutStream)))) {
        throw std::runtime_error(format("Unable to open audio {} device: ", to_string(io)) + soundio_strerror(err));
    }

    if (io == IO_In && InStream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(InStream->layout_error); }
    else if (io == IO_Out && OutStream->layout_error) { cerr << "Unable to set " << io << " channel layout: " << soundio_strerror(OutStream->layout_error); }
}
static void StartStream(const IO io) {
    if (io == IO_None) return;

    int err;
    if ((err = (io == IO_In ? soundio_instream_start(InStream) : soundio_outstream_start(OutStream)))) {
        throw std::runtime_error(format("Unable to start audio {} device: ", to_string(io)) + soundio_strerror(err));
    }
}
static void DestroyStream(const IO io) {
    if (io == IO_None) return;

    if (io == IO_In) soundio_instream_destroy(InStream);
    else soundio_outstream_destroy(OutStream);

    soundio_device_unref(Devices[io]);
}

static float MicLatency = 0.2; // Seconds todo do better than this guess
static int UnderflowCount = 0, LastReadFrameCount = 0, LastWriteFrameCount = 0;
static bool SoundIoReady = false, FaustReady = false;

// Faust vars

// Used to initialize the static Faust buffer.
// This is the highest `max_frames` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frames at the transition.
// If it needs bumping up, bump away!
static constexpr int FaustBufferFrames = 2048;

static llvm_dsp_factory *DspFactory;
static dsp *FaustDsp = nullptr;
static Box FaustBox = nullptr;
static unique_ptr<FaustUI> FaustUi;

void SetupAudio() {
    // Local copies for bookkeeping in read/write callbacks, so others can safely use the global `Areas` pointers above (e.g. for charting):
    static char *area_pointers[2][SOUNDIO_MAX_CHANNELS];

    soundio = soundio_create();
    if (!soundio) throw std::runtime_error("Out of memory");

    int err = s.Audio.Backend == AudioBackend::none ? soundio_connect(soundio) : soundio_connect_backend(soundio, ToSoundIoBackend(s.Audio.Backend));
    if (err) throw std::runtime_error("Unable to connect to backend: "s + soundio_strerror(err));

    soundio_flush_events(soundio);

    // Input/output device setup
    for (const IO io: IO_All) {
        DeviceIds[io].clear();
        SupportedFormats[io].clear();
        SupportedSampleRates[io].clear();

        int default_device_index = GetDefaultDeviceIndex(io);
        if (default_device_index < 0) throw std::runtime_error(format("No audio {} device found", to_string(io))); // todo move on without input

        const auto device_count = GetDeviceCount(io);
        for (int i = 0; i < device_count; i++) DeviceIds[io].emplace_back(GetDevice(io, i)->id);

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
        if (device->probe_error) throw std::runtime_error("Cannot probe device: "s + soundio_strerror(device->probe_error));

        for (int i = 0; i < device->sample_rate_count; i++) SupportedSampleRates[io].push_back(device->sample_rates[i].max);
        if (SupportedSampleRates[io].empty()) throw std::runtime_error(format("{} audio stream has no supported sample rates", Capitalize(to_string(io))));

        Devices[io] = device;
        CreateStream(io);
    }

    // This is from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_microphone.c#L308-L313,
    // but it fails with a mono microphone and stereo output, which is a common scenario that we'll happily handle.
//    soundio_device_sort_channel_layouts(out_device);
//    const auto *layout = soundio_best_matching_channel_layout(out_device->layouts, out_device->layout_count, in_device->layouts, in_device->layout_count);
//    if (!layout) throw std::runtime_error("Channel layouts not compatible");
//    InStream->layout = *layout;
//    OutStream->layout = *layout;

    InStream->read_callback = [](SoundIoInStream *InStream, int min_frames, int max_frames) {
        const auto channel_count = InStream->layout.channel_count;
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
            if ((err = soundio_instream_begin_read(InStream, &Areas[IO_In], &inner_frames))) {
                cerr << "Begin read error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            LastReadFrameCount = inner_frames;
            if (!inner_frames) break;

            if (!Areas[IO_In]) {
                // Due to an overflow there is a hole. Fill the hole in the ring buffer with silence.
                memset(write_ptr_direct, 0, inner_frames * SampleSize * channel_count);
                cerr << format("Dropped {} frames due to internal overflow", inner_frames) << '\n';
            } else {
                // Make a local copy of the input area pointers for incrementing, so others can read directly from the device areas.
                // todo Others assume doubles, so we'll need indirect converted buffers when the input stream uses a different format.
                for (int channel = 0; channel < channel_count; channel += 1) area_pointers[IO_In][channel] = Areas[IO_In][channel].ptr;

                for (int frame = 0; frame < inner_frames; frame += 1) {
                    for (int channel = 0; channel < channel_count; channel += 1) {
                        *write_ptr_direct = ReadSample(area_pointers[IO_In][channel]);
                        area_pointers[IO_In][channel] += Areas[IO_In][channel].step;
                        write_ptr_direct += 1;
                    }
                }
            }
            soundio_ring_buffer_advance_write_ptr(InputBufferDirect, inner_frames * SampleSize * channel_count);

            if ((err = soundio_instream_end_read(InStream))) {
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
            const int resample_input_bytes = available_resample_read_frames * SampleSize * InStream->layout.channel_count;
            soundio_ring_buffer_advance_read_ptr(InputBufferDirect, resample_input_bytes);

            if (resampled_frames > 0) {
                const int output_bytes = resampled_frames * SampleSize * channel_count;
                auto *write_ptr = (Sample *) soundio_ring_buffer_write_ptr(InputBuffer);
                memcpy(write_ptr, resampled_buffer, output_bytes);
                soundio_ring_buffer_advance_write_ptr(InputBuffer, output_bytes);
            }
        }
    };

    OutStream->write_callback = [](SoundIoOutStream *OutStream, int /*min_frames*/, int max_frames) {
        const auto channel_count = OutStream->layout.channel_count;
        const int input_sample_count = soundio_ring_buffer_fill_count(InputBuffer) / SampleSize;
        const bool faust_running = s.Audio.FaustRunning;

        int err;
        int remaining_frames = max_frames;
        while (remaining_frames > 0) {
            int inner_frames = remaining_frames;
            if ((err = soundio_outstream_begin_write(OutStream, &Areas[IO_Out], &inner_frames))) {
                cerr << "Begin write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            if (faust_running && FaustReady) {
                if (inner_frames > FaustBufferFrames) {
                    cerr << "The Faust output buffer only has " << FaustBufferFrames << " frames, which is less than the required " << inner_frames << ".\n"
                         << "(Increase `Audio.cpp::FaustBufferFrames`.)\n";
                }

                if (FaustDsp->getNumInputs() > 0 && GetChannelCount(IO_In) == 1) {
                    auto *read_ptr = (Sample *) soundio_ring_buffer_read_ptr(InputBuffer);
                    // Point every Faust input channel to the first audio input channel.
                    // Not advancing the input read pointer until after any input monitoring below.
                    // todo this works for 1:1 channel case, but need to properly handle input routing between more channels on either side
                    for (int i = 0; i < FaustDsp->getNumInputs(); i++) FaustBuffers[IO_In][i] = read_ptr;
                }

                if (FaustReady) FaustDsp->compute(inner_frames, FaustBuffers[IO_In], FaustBuffers[IO_Out]);
            }

            LastWriteFrameCount = inner_frames;
            if (inner_frames <= 0) break;

            // Make a local copy of the output area pointers for incrementing, so others can read directly from the device areas.
            // todo Others assume doubles, so we'll need indirect converted buffers when the output stream uses a different format.
            for (int channel = 0; channel < channel_count; channel += 1) area_pointers[IO_Out][channel] = Areas[IO_Out][channel].ptr;

            auto *read_ptr = (Sample *) soundio_ring_buffer_read_ptr(InputBuffer);
            for (int inner_frame = 0; inner_frame < inner_frames; inner_frame++) {
                for (int channel = 0; channel < channel_count; channel += 1) {
                    Sample out_sample = 0;
                    if (!s.Audio.Muted) {
                        // Monitor input directly from the ring buffer.
                        if (s.Audio.MonitorInput) out_sample += *read_ptr;
                        if (faust_running && FaustReady) {
                            const int outer_frame = max_frames - remaining_frames + inner_frame;
                            out_sample += FaustBuffers[IO_Out][min(channel, FaustDsp->getNumOutputs() - 1)][outer_frame];
                        }
                    }

                    WriteSample(area_pointers[IO_Out][channel], out_sample);
                    area_pointers[IO_Out][channel] += Areas[IO_Out][channel].step;
                }
                read_ptr += 1; // todo xxx this assumes mono input!
            }
            soundio_ring_buffer_advance_read_ptr(InputBuffer, min(input_sample_count, inner_frames) * SampleSize);

            if ((err = soundio_outstream_end_write(OutStream))) {
                if (err == SoundIoErrorUnderflow) return;
                cerr << "End write error: " << soundio_strerror(err) << '\n';
                exit(1);
            }

            remaining_frames -= inner_frames;
        }
    };

    OutStream->underflow_callback = [](SoundIoOutStream *) { cerr << "Underflow #" << UnderflowCount++ << '\n'; };

    for (IO io: IO_All) OpenStream(io);

    // Set up resampler if needed.
    if (InStream->sample_rate != OutStream->sample_rate) {
        Resampler = make_unique<r8b::CDSPResampler24>(InStream->sample_rate, OutStream->sample_rate, 1024); // todo can we get max frame size here?
    }

    // Initialize the input ring buffer(s).
    InputBufferDirect = soundio_ring_buffer_create(soundio, int(ceil(MicLatency * 2 * float(InStream->sample_rate)) * SampleSize));
    if (!InputBufferDirect) throw std::runtime_error("Unable to create direct input buffer: Out of memory");

    if (InStream->sample_rate == OutStream->sample_rate) {
        InputBuffer = InputBufferDirect;
    } else {
        InputBuffer = soundio_ring_buffer_create(soundio, int(ceil(MicLatency * 2 * float(OutStream->sample_rate)) * SampleSize));
        if (!InputBuffer) throw std::runtime_error("Unable to create input buffer: Out of memory");
    }

    ReadSample = ReadSampleForFormat(InStream->format);
    WriteSample = WriteSampleForFormat(OutStream->format);

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

static string PreviousFaustCode;
static int PreviousFaustSampleRate = 0;

void Audio::UpdateProcess() const {
    if (Running && !soundio) {
        RetryingSetupAudio();
    } else if (!Running && soundio) {
        TeardownAudio();
    } else if (SoundIoReady &&
        (InStream->device->id != InDeviceId || OutStream->device->id != OutDeviceId ||
            InStream->sample_rate != s.Audio.InSampleRate || OutStream->sample_rate != OutSampleRate ||
            InStream->format != ToSoundIoFormat(InFormat) || OutStream->format != ToSoundIoFormat(OutFormat))) {
        // Reset to make any audio config changes take effect.
        TeardownAudio();
        RetryingSetupAudio();
    }

    static bool first_run = true;
    if (first_run) {
        first_run = false;

        static StoreEntries values;
        if (InStream->device->id != InDeviceId) values.emplace_back(InDeviceId.Path, InStream->device->id);
        if (OutStream->device->id != OutDeviceId) values.emplace_back(OutDeviceId.Path, OutStream->device->id);
        if (InStream->sample_rate != InSampleRate) values.emplace_back(InSampleRate.Path, InStream->sample_rate);
        if (OutStream->sample_rate != OutSampleRate) values.emplace_back(OutSampleRate.Path, OutStream->sample_rate);
        if (InStream->format != InFormat) values.emplace_back(InFormat.Path, ToAudioFormat(InStream->format));
        if (OutStream->format != OutFormat) values.emplace_back(OutFormat.Path, ToAudioFormat(OutStream->format));
        if (!values.empty()) q(set_values{values}, true);
    }

    if (Faust.Code != PreviousFaustCode || OutSampleRate != PreviousFaustSampleRate) {
        PreviousFaustCode = string(Faust.Code);
        PreviousFaustSampleRate = OutSampleRate;

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
            FaustBox = DSPToBoxes("FlowGrid", Faust.Code, argc, argv, &num_inputs, &num_outputs, error_msg);
            if (FaustBox && error_msg.empty()) {
                static const int optimize_level = -1;
                DspFactory = createDSPFactoryFromBoxes("FlowGrid", FaustBox, argc, argv, "", error_msg, optimize_level);
            }
            if (!FaustBox && error_msg.empty()) error_msg = "`DSPToBoxes` returned no error but did not produce a result.";
        }
        if (DspFactory && error_msg.empty()) {
            FaustDsp = DspFactory->createDSPInstance();
            FaustDsp->init(OutSampleRate);

            // Init `FaustBuffers`
            for (const IO io: IO_All) {
                const int channels = io == IO_In ? FaustDsp->getNumInputs() : FaustDsp->getNumOutputs();
                if (channels > 0) FaustBuffers[io] = new Sample *[channels];
            }
            for (int i = 0; i < FaustDsp->getNumOutputs(); i++) { FaustBuffers[IO_Out][i] = new Sample[FaustBufferFrames]; }

            FaustReady = true;
            FaustUi = make_unique<FaustUI>();
            FaustDsp->buildUserInterface(FaustUi.get());
        } else {
            FaustUi = nullptr;
            FaustReady = false;

            if (FaustDsp) {
                // Destroy `FaustBuffers`
                for (int i = 0; i < FaustDsp->getNumOutputs(); i++) { delete[] FaustBuffers[IO_Out][i]; }
                for (const IO io: IO_All) {
                    delete[] FaustBuffers[io];
                    FaustBuffers[io] = nullptr;
                }
                delete FaustDsp;
                FaustDsp = nullptr;
                deleteDSPFactory(DspFactory);
                DspFactory = nullptr;
            }
        }

        if (!error_msg.empty()) q(set_value{Faust.Error.Path, error_msg});
        else if (Faust.Error) q(set_value{Faust.Error.Path, ""});

        OnBoxChange(FaustBox);
        OnUiChange(FaustUi.get());
    }

    if (SoundIoReady && OutStream->volume != OutDeviceVolume) soundio_outstream_set_volume(OutStream, OutDeviceVolume);
}

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
        const string &io_label = Capitalize(to_string(io));
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
        BulletText("Name: %s", InStream->name);
        BulletText("Device ID: %s", InStream->device->id);
        BulletText("Format: %s", soundio_format_string(InStream->format));
        BulletText("Sample rate: %d", InStream->sample_rate);
        if (TreeNodeEx("Channel layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ShowChannelLayout(InStream->layout, false);
            TreePop();
        }
        BulletText("Software latency: %0.8f sec", InStream->software_latency);
        BulletText("Bytes per frame: %d", InStream->bytes_per_frame);
        BulletText("Bytes per sample: %d", InStream->bytes_per_sample);

        TreePop();
    }
    if (TreeNode("Output stream")) {
        BulletText("Name: %s", OutStream->name);
        BulletText("Device ID: %s", OutStream->device->id);
        BulletText("Format: %s", soundio_format_string(OutStream->format));
        BulletText("Sample rate: %d", OutStream->sample_rate);
        if (TreeNodeEx("Channel layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ShowChannelLayout(OutStream->layout, false);
            TreePop();
        }
        BulletText("Volume: %0.8f", OutStream->volume);
        BulletText("Software latency: %0.8f sec", OutStream->software_latency);
        BulletText("Bytes per frame: %d", OutStream->bytes_per_frame);
        BulletText("Bytes per sample: %d", OutStream->bytes_per_sample);

        TreePop();
    }
}

void ShowBufferPlots() {
    for (IO io: IO_All) {
        const bool is_in = io == IO_In;
        if (TreeNode(Capitalize(to_string(io)).c_str())) {
            const auto *area = is_in ? Areas[IO_In] : Areas[IO_Out];
            if (!area) continue;

            const auto *device = is_in ? InStream->device : OutStream->device;
            const auto &layout = is_in ? InStream->layout : OutStream->layout;
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

void Audio::Render() const {
    Running.Draw();
    if (!SoundIoReady) {
        TextUnformatted("No audio context created yet");
        return;
    }

    FaustRunning.Draw();
    Muted.Draw();
    MonitorInput.Draw();
    OutDeviceVolume.Draw();

    if (!DeviceIds[IO_In].empty()) InDeviceId.Render(DeviceIds[IO_In]);
    if (!DeviceIds[IO_Out].empty()) OutDeviceId.Render(DeviceIds[IO_Out]);
    if (!SupportedFormats[IO_In].empty()) InFormat.Render(SupportedFormats[IO_In]);
    if (!SupportedFormats[IO_Out].empty()) OutFormat.Render(SupportedFormats[IO_Out]);
    if (!SupportedSampleRates[IO_In].empty()) InSampleRate.Render(SupportedSampleRates[IO_In]);
    if (!SupportedSampleRates[IO_Out].empty()) OutSampleRate.Render(SupportedSampleRates[IO_Out]);
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
