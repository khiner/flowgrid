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

const vector<int> MiniAudio::PrioritizedDefaultSampleRates = {48000, 44100, 96000};
const vector<MiniAudio::IoFormat> MiniAudio::PrioritizedDefaultFormats = {
    IoFormat_F32,
    IoFormat_S32,
    IoFormat_S16,
    IoFormat_Invalid,
};

namespace MiniAudioN {
ma_format ToMiniAudioFormat(const MiniAudio::IoFormat format) {
    switch (format) {
        case MiniAudio::IoFormat_Invalid: return ma_format_unknown;
        case MiniAudio::IoFormat_F32: return ma_format_f32;
        case MiniAudio::IoFormat_S32: return ma_format_s32;
        case MiniAudio::IoFormat_S24: return ma_format_s24;
        case MiniAudio::IoFormat_S16: return ma_format_s16;
        case MiniAudio::IoFormat_U8: return ma_format_u8;
        default: return ma_format_unknown;
    }
}
MiniAudio::IoFormat ToAudioFormat(const ma_format format) {
    switch (format) {
        case ma_format_unknown: return MiniAudio::IoFormat_Invalid;
        case ma_format_f32: return MiniAudio::IoFormat_F32;
        case ma_format_s32: return MiniAudio::IoFormat_S32;
        case ma_format_s24: return MiniAudio::IoFormat_S24;
        case ma_format_s16: return MiniAudio::IoFormat_S16;
        case ma_format_u8: return MiniAudio::IoFormat_U8;
        default: return MiniAudio::IoFormat_Invalid;
    }
}

// Faust vars

// Used to initialize the static Faust buffer.
// This is the highest `max_frames` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frames at the transition.
// If it needs bumping up, bump away!
static constexpr int FaustBufferFrames = 2048;

static bool FaustReady = false;
static Sample **FaustBuffers[IO_Count];
static llvm_dsp_factory *DspFactory;
static dsp *FaustDsp = nullptr;
static Box FaustBox = nullptr;
static unique_ptr<FaustParams> FaustUi;
static int PreviousFaustSampleRate = 0;

static string PreviousFaustCode;
static string PreviousInDeviceName, PreviousOutDeviceName;
static int PreviousInSampleRate, PreviousOutSampleRate;
static int PreviousInFormat, PreviousOutFormat;
static float PreviousOutDeviceVolume;

} // namespace MiniAudioN

using namespace MiniAudioN;

void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    MA_ASSERT(device->capture.format == device->playback.format);
    MA_ASSERT(device->capture.channels == device->playback.channels);

    /* In this example the format and channel count are the same for both input and output which means we can just memcpy(). */
    MA_COPY_MEMORY(output, input, frame_count * ma_get_bytes_per_frame(device->capture.format, device->capture.channels));
}

// Context
static ma_context AudioContext;
static bool AudioContextInitialized = false;
static vector<ma_device_info *> DeviceInfos[IO_Count];
// Derived from above (todo combine)
static vector<string> DeviceNames[IO_Count];

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
        DeviceNames[io].clear();
    }

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
}

void MiniAudio::InitDevice() const {
    if (!AudioContextInitialized) Init(); // todo explicit re-scan action

    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ma_format_s16;
    DeviceConfig.capture.channels = 2;
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ma_format_s16;
    DeviceConfig.playback.channels = 2;
    DeviceConfig.dataCallback = DataCallback;

    auto result = ma_device_init(NULL, &DeviceConfig, &Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio device: {}", result));

    ma_context_get_device_info(Device.pContext, Device.type, nullptr, &DeviceInfo);
    result = ma_device_start(&Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", result));
}

void MiniAudio::TeardownDevice() const {
    ma_device_uninit(&Device);
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
         PreviousInSampleRate != InSampleRate || PreviousOutSampleRate != OutSampleRate ||
         PreviousInFormat != InFormat || PreviousOutFormat != OutFormat)
    ) {
        PreviousInDeviceName = string(InDeviceName);
        PreviousOutDeviceName = string(OutDeviceName);
        PreviousInSampleRate = InSampleRate;
        PreviousOutSampleRate = OutSampleRate;
        PreviousInFormat = InFormat;
        PreviousOutFormat = OutFormat;
        // Reset to make any audio config changes take effect.
        TeardownDevice();
        InitDevice();
    }

    static bool first_run = true;
    if (first_run) {
        first_run = false;

        static StoreEntries values;
        // if (InStream->device->id != InDeviceId) values.emplace_back(InDeviceId.Path, InStream->device->id);
        // if (OutStream->device->id != OutDeviceId) values.emplace_back(OutDeviceId.Path, OutStream->device->id);
        // if (InStream->sample_rate != InSampleRate) values.emplace_back(InSampleRate.Path, InStream->sample_rate);
        // if (OutStream->sample_rate != OutSampleRate) values.emplace_back(OutSampleRate.Path, OutStream->sample_rate);
        // if (InStream->format != InFormat) values.emplace_back(InFormat.Path, ToAudioFormat(InStream->format));
        // if (OutStream->format != OutFormat) values.emplace_back(OutFormat.Path, ToAudioFormat(OutStream->format));
        // if (!values.empty()) q(SetValues{values}, true);
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

namespace MiniAudioN {
} // namespace MiniAudioN

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

    if (!DeviceNames[IO_In].empty()) InDeviceName.Render(DeviceNames[IO_In]);
    if (!DeviceNames[IO_Out].empty()) OutDeviceName.Render(DeviceNames[IO_Out]);
    // if (!SupportedFormats[IO_In].empty()) InFormat.Render(SupportedFormats[IO_In]);
    // if (!SupportedFormats[IO_Out].empty()) OutFormat.Render(SupportedFormats[IO_Out]);
    // if (!SupportedSampleRates[IO_In].empty()) InSampleRate.Render(SupportedSampleRates[IO_In]);
    // if (!SupportedSampleRates[IO_Out].empty()) OutSampleRate.Render(SupportedSampleRates[IO_Out]);
    // NewLine();
    // if (TreeNode("Devices")) {
    //     ShowDevices();
    //     TreePop();
    // }
    // if (TreeNode("Streams")) {
    //     ShowStreams();
    //     TreePop();
    // }
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
