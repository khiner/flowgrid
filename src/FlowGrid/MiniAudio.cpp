// Adapted from:
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_sine.c and
// * https://github.com/andrewrk/libsoundio/blob/master/example/sio_microphone.c

#include <thread> // For sleep fn

#include "CDSPResampler.h"
#include "Helper/Sample.h" // Must be included before any Faust includes
#include "faust/dsp/llvm-dsp.h"

#include "App.h"
#include "Helper/File.h"
#include "UI/Faust/FaustGraph.h"
#include "UI/Faust/FaustParams.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// todo support loopback mode? (think of use cases)

const vector<MiniAudio::IoFormat> MiniAudio::PrioritizedDefaultFormats = {
    IoFormat_F32,
    IoFormat_S32,
    IoFormat_S16,
    IoFormat_S24,
    IoFormat_U8,
    IoFormat_Native,
};

ma_format ToMiniAudioFormat(const MiniAudio::IoFormat format) {
    switch (format) {
        case MiniAudio::IoFormat_Native: return ma_format_unknown;
        case MiniAudio::IoFormat_F32: return ma_format_f32;
        case MiniAudio::IoFormat_S32: return ma_format_s32;
        case MiniAudio::IoFormat_S16: return ma_format_s16;
        case MiniAudio::IoFormat_S24: return ma_format_s24;
        case MiniAudio::IoFormat_U8: return ma_format_u8;
        default: return ma_format_unknown;
    }
}
MiniAudio::IoFormat ToAudioFormat(const ma_format format) {
    switch (format) {
        case ma_format_unknown: return MiniAudio::IoFormat_Native;
        case ma_format_f32: return MiniAudio::IoFormat_F32;
        case ma_format_s32: return MiniAudio::IoFormat_S32;
        case ma_format_s16: return MiniAudio::IoFormat_S16;
        case ma_format_s24: return MiniAudio::IoFormat_S24;
        case ma_format_u8: return MiniAudio::IoFormat_U8;
        default: return MiniAudio::IoFormat_Native;
    }
}

const string MiniAudio::GetFormatName(int format_index) {
    return ma_get_format_name(ToMiniAudioFormat(format_index));
}

// Used to initialize the static Faust buffer.
// This is the highest `max_frames` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frames at the transition.
// If it needs bumping up, bump away!
static constexpr int FaustBufferFrames = 2048;

// Faust vars:
static bool FaustReady = false;
static Sample **FaustBuffers[IO_Count];
static llvm_dsp_factory *DspFactory;
static dsp *FaustDsp = nullptr;
static Box FaustBox = nullptr;
static unique_ptr<FaustParams> FaustUi;
static U32 PreviousFaustSampleRate = 0;

// State value cache vars:
static string PreviousFaustCode;
static string PreviousInDeviceName, PreviousOutDeviceName;
static MiniAudio::IoFormat PreviousInFormat, PreviousOutFormat;
static U32 PreviousSampleRate;
static float PreviousOutDeviceVolume;

void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    if (device->capture.channels == device->playback.channels) {
        // If the formats are the same for both input and output, `ma_convert_pcm` just does a `memcpy`.
        ma_convert_pcm_frames_format(output, device->playback.format, input, device->capture.format, frame_count, device->capture.channels, ma_dither_mode_none);
    } else {
        // const auto result = ma_data_converter_process_pcm_frames(&converter, input, &frame_count, output, &frame_count);
    }
}

// Context
static ma_context AudioContext;
static bool AudioContextInitialized = false;
static vector<ma_device_info *> DeviceInfos[IO_Count];
// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;

// Derived from above (todo combine)
static vector<string> DeviceNames[IO_Count];
static vector<MiniAudio::IoFormat> DeviceFormats[IO_Count];
static vector<ma_uint32> DeviceSampleRates; // Sample rates shared between

// Current device
static ma_device_config DeviceConfig;
static ma_device Device;
static ma_device_info DeviceInfo;

static inline bool IsDeviceStarted() {
    return ma_device_is_started(&Device);
}

static const ma_device_id *GetDeviceId(IO io, string_view device_name) {
    const auto &infos = DeviceInfos[io];
    for (const ma_device_info *info : infos) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

void MiniAudio::Init() const {
    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
        DeviceFormats[io].clear();
    }
    DeviceSampleRates.clear();

    auto result = ma_context_init(NULL, 0, NULL, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio context: {}", result));

    static ma_uint32 PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio devices: {}", result));

    for (ma_uint32 i = 0; i < CaptureDeviceCount; i++) {
        DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
    }
    for (ma_uint32 i = 0; i < PlaybackDeviceCount; i++) {
        DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
        DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
    }

    for (const IO io : IO_All) {
        for (const auto format : MiniAudio::PrioritizedDefaultFormats) {
            // DeviceInfo.nativeDataFormats
            DeviceFormats[io].emplace_back(format); // miniaudio supports automatic conversion to/from any format. Use ma_format_unknown for native format.
            // todo don't show an additional 'native' option. Instead, highlight/mark the native formats on the current device.
            //   Aldo, don't clear/re-fill `DeviceFormats`. Just render all of them no matter what and mark the native ones.
        }
    }
    for (const auto sample_rate : g_maStandardSampleRatePriorities) {
        DeviceSampleRates.emplace_back(sample_rate);
    }
}

// todo implement for r8brain resampler
// todo I want to use this currently to support quality/fast resampling between _natively supported_ device sample rates.
//   Can I still use duplex mode in this case?
// static ma_resampling_backend_vtable ResamplerVTable = {
//     ma_resampling_backend_get_heap_size__linear,
//     ma_resampling_backend_init__linear,
//     ma_resampling_backend_uninit__linear,
//     ma_resampling_backend_process__linear,
//     ma_resampling_backend_set_rate__linear,
//     ma_resampling_backend_get_input_latency__linear,
//     ma_resampling_backend_get_output_latency__linear,
//     ma_resampling_backend_get_required_input_frame_count__linear,
//     ma_resampling_backend_get_expected_output_frame_count__linear,
//     ma_resampling_backend_reset__linear,
// };

void MiniAudio::InitDevice() const {
    if (!AudioContextInitialized) Init(); // todo explicit re-scan action

    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ToMiniAudioFormat(InFormat);
    DeviceConfig.capture.channels = 2;
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ToMiniAudioFormat(OutFormat);
    DeviceConfig.playback.channels = 2;
    DeviceConfig.dataCallback = DataCallback;
    DeviceConfig.sampleRate = SampleRate;

    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    auto result = ma_device_init(NULL, &DeviceConfig, &Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(Device.pContext, Device.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio device info: {}", result));

    result = ma_device_start(&Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", result));
}

void MiniAudio::TeardownDevice() const {
    ma_device_uninit(&Device);
    // ma_resampler_uninit(&Resampler, nullptr);
}

// todo still need to call this on app shutdown.
void MiniAudio::Teardown() const {
    const auto result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error shutting down audio context: {}", result));
    AudioContextInitialized = false;
}

void MiniAudio::UpdateProcess() const {
    if (Running && !IsDeviceStarted()) {
        InitDevice();
    } else if (!Running && IsDeviceStarted()) {
        TeardownDevice();
    } else if (
        IsDeviceStarted() &&
        (PreviousInDeviceName != InDeviceName || PreviousOutDeviceName != OutDeviceName ||
         PreviousInFormat != InFormat || PreviousOutFormat != OutFormat ||
         PreviousSampleRate != SampleRate)
    ) {
        PreviousInDeviceName = string(InDeviceName);
        PreviousOutDeviceName = string(OutDeviceName);
        PreviousInFormat = InFormat;
        PreviousOutFormat = OutFormat;
        PreviousSampleRate = SampleRate;
        // Reset to make any audio config changes take effect.
        // todo no need to completely reset in many cases (like just format changes). Just reset the data_converter.
        TeardownDevice();
        InitDevice();
    }

    // Initialize state values to reflect the initial device configuration
    static bool first_run = true;
    if (first_run) {
        first_run = false;

        static StoreEntries initial_settings;
        if (Device.capture.name != InDeviceName) initial_settings.emplace_back(InDeviceName.Path, Device.capture.name);
        if (Device.playback.name != OutDeviceName) initial_settings.emplace_back(OutDeviceName.Path, Device.playback.name);
        if (Device.capture.format != InFormat) initial_settings.emplace_back(InFormat.Path, ToAudioFormat(Device.capture.format));
        if (Device.playback.format != OutFormat) initial_settings.emplace_back(OutFormat.Path, ToAudioFormat(Device.playback.format));
        if (Device.sampleRate != SampleRate) initial_settings.emplace_back(SampleRate.Path, Device.sampleRate);
        if (!initial_settings.empty()) q(SetValues{initial_settings}, true);
    }

    if (Faust.Code != PreviousFaustCode || SampleRate != PreviousFaustSampleRate) {
        PreviousFaustCode = string(Faust.Code);
        PreviousFaustSampleRate = SampleRate;

        string error_msg;
        destroyLibContext();
        if (Faust.Code && SampleRate) {
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
            FaustDsp->init(SampleRate);

            // Init `FaustBuffers`
            for (const IO io : IO_All) {
                const int channels = io == IO_In ? FaustDsp->getNumInputs() : FaustDsp->getNumOutputs();
                if (channels > 0) FaustBuffers[io] = new Sample *[channels];
            }
            for (int i = 0; i < FaustDsp->getNumOutputs(); i++) { FaustBuffers[IO_Out][i] = new Sample[FaustBufferFrames]; }

            FaustReady = true;
            FaustUi = make_unique<FaustParams>();
            FaustDsp->buildUserInterface(FaustUi.get());
        } else {
            FaustUi = nullptr;
            FaustReady = false;

            if (FaustDsp) {
                // Destroy `FaustBuffers`
                for (int i = 0; i < FaustDsp->getNumOutputs(); i++) { delete[] FaustBuffers[IO_Out][i]; }
                for (const IO io : IO_All) {
                    delete[] FaustBuffers[io];
                    FaustBuffers[io] = nullptr;
                }
                delete FaustDsp;
                FaustDsp = nullptr;
                deleteDSPFactory(DspFactory);
                DspFactory = nullptr;
            }
        }

        if (!error_msg.empty()) q(SetValue{Faust.Log.Error.Path, error_msg});
        else if (Faust.Log.Error) q(SetValue{Faust.Log.Error.Path, ""});

        OnBoxChange(FaustBox);
        OnUiChange(FaustUi.get());
    }

    if (IsDeviceStarted() && PreviousOutDeviceVolume != OutDeviceVolume) ma_device_set_master_volume(&Device, OutDeviceVolume);
}

using namespace ImGui;

static void DrawDevice(ma_device *device) {
    assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

    Text("[%s]", ma_get_backend_name(device->pContext->backend));

    static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
    ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), NULL);
    if (TreeNode(format("{} ({})", name, "Capture").c_str())) {
        Text("Format: %s -> %s", ma_get_format_name(device->capture.internalFormat), ma_get_format_name(device->capture.format));
        Text("Channels: %d -> %d", device->capture.internalChannels, device->capture.channels);
        Text("Sample Rate: %d -> %d", device->capture.internalSampleRate, device->sampleRate);
        Text("Buffer Size: %d*%d (%d)\n", device->capture.internalPeriodSizeInFrames, device->capture.internalPeriods, (device->capture.internalPeriodSizeInFrames * device->capture.internalPeriods));
        if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen, "Conversion")) {
            Text("Pre Format Conversion: %s\n", device->capture.converter.hasPreFormatConversion ? "YES" : "NO");
            Text("Post Format Conversion: %s\n", device->capture.converter.hasPostFormatConversion ? "YES" : "NO");
            Text("Channel Routing: %s\n", device->capture.converter.hasChannelConverter ? "YES" : "NO");
            Text("Resampling: %s\n", device->capture.converter.hasResampler ? "YES" : "NO");
            Text("Passthrough: %s\n", device->capture.converter.isPassthrough ? "YES" : "NO");
            {
                char channel_map[1024];
                ma_channel_map_to_string(device->capture.internalChannelMap, device->capture.internalChannels, channel_map, sizeof(channel_map));
                Text("Channel Map In: {%s}\n", channel_map);

                ma_channel_map_to_string(device->capture.channelMap, device->capture.channels, channel_map, sizeof(channel_map));
                Text("Channel Map Out: {%s}\n", channel_map);
            }
            TreePop();
        }
        TreePop();
    }

    if (device->type == ma_device_type_loopback) return;

    ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), NULL);
    if (TreeNode(format("{} ({})", name, "Playback").c_str())) {
        Text("Format: %s -> %s", ma_get_format_name(device->playback.format), ma_get_format_name(device->playback.internalFormat));
        Text("Channels: %d -> %d", device->playback.channels, device->playback.internalChannels);
        Text("Sample Rate: %d -> %d", device->sampleRate, device->playback.internalSampleRate);
        Text("Buffer Size: %d*%d (%d)", device->playback.internalPeriodSizeInFrames, device->playback.internalPeriods, (device->playback.internalPeriodSizeInFrames * device->playback.internalPeriods));
        if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen, "Conversion")) {
            Text("Pre Format Conversion:  %s", device->playback.converter.hasPreFormatConversion ? "YES" : "NO");
            Text("Post Format Conversion: %s", device->playback.converter.hasPostFormatConversion ? "YES" : "NO");
            Text("Channel Routing: %s", device->playback.converter.hasChannelConverter ? "YES" : "NO");
            Text("Resampling: %s", device->playback.converter.hasResampler ? "YES" : "NO");
            Text("Passthrough: %s", device->playback.converter.isPassthrough ? "YES" : "NO");
            {
                char channel_map[1024];
                ma_channel_map_to_string(device->playback.channelMap, device->playback.channels, channel_map, sizeof(channel_map));
                Text("Channel Map In: {%s}", channel_map);

                ma_channel_map_to_string(device->playback.internalChannelMap, device->playback.internalChannels, channel_map, sizeof(channel_map));
                Text("Channel Map Out: {%s}", channel_map);
            }
            TreePop();
        }
        TreePop();
    }
}

// void DrawDevices() {
//     for (const IO io : IO_All) {
//         const Count device_count = GetDeviceCount(io);
//         if (TreeNodeEx(format("{} devices ({})", Capitalize(to_string(io)), device_count).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
//             for (Count device_index = 0; device_index < device_count; device_index++) {
//                 auto *device = GetDevice(io, device_index);
//                 ShowDevice(*device);
//             }
//             TreePop();
//         }
//     }
// }
void DrawDevices() {
    DrawDevice(&Device);
}

void MiniAudio::Render() const {
    Running.Draw();
    if (!IsDeviceStarted()) {
        TextUnformatted("No audio device started yet");
        return;
    }

    FaustRunning.Draw();
    Muted.Draw();
    MonitorInput.Draw();
    OutDeviceVolume.Draw();
    SampleRate.Render(DeviceSampleRates);

    for (const IO io : IO_All) {
        NewLine();
        TextUnformatted(Capitalize(to_string(io)).c_str());
        (io == IO_In ? InDeviceName : OutDeviceName).Render(DeviceNames[io]);
        (io == IO_In ? InFormat : OutFormat).Render(DeviceFormats[io]);
    }

    NewLine();
    if (TreeNode("Devices")) {
        DrawDevices();
        TreePop();
    }
    // const auto backend_count = soundio_backend_count(soundio);
    // if (TreeNodeEx("Backends", ImGuiTreeNodeFlags_None, "Available backends (%d)", backend_count)) {
    //     for (int i = 0; i < backend_count; i++) {
    //         const auto backend = soundio_get_backend(soundio, i);
    //         BulletText("%s%s", soundio_backend_name(backend), backend == soundio->current_backend ? " (current)" : "");
    //     }
    //     TreePop();
    // }
    // if (TreeNode("Plots")) {
    //     ShowBufferPlots();
    //     TreePop();
    // }

    Faust.Draw();
}
