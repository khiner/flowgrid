// #include "CDSPResampler.h"
#include "Helper/Sample.h" // Must be included before any Faust includes
#include "faust/dsp/llvm-dsp.h"

#include "App.h"
#include "Helper/File.h"
#include "UI/Faust/FaustGraph.h"
#include "UI/Faust/FaustParams.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// todo support loopback mode? (think of use cases)

const vector<Audio::IoFormat> Audio::PrioritizedFormats = {
    IoFormat_F32,
    IoFormat_S32,
    IoFormat_S16,
    IoFormat_S24,
    IoFormat_U8,
};
const vector<U32> Audio::PrioritizedSampleRates = {std::begin(g_maStandardSampleRatePriorities), std::end(g_maStandardSampleRatePriorities)};

ma_format ToAudioFormat(const Audio::IoFormat format) {
    switch (format) {
        case Audio::IoFormat_Native: return ma_format_unknown;
        case Audio::IoFormat_F32: return ma_format_f32;
        case Audio::IoFormat_S32: return ma_format_s32;
        case Audio::IoFormat_S16: return ma_format_s16;
        case Audio::IoFormat_S24: return ma_format_s24;
        case Audio::IoFormat_U8: return ma_format_u8;
        default: return ma_format_unknown;
    }
}
Audio::IoFormat ToAudioFormat(const ma_format format) {
    switch (format) {
        case ma_format_unknown: return Audio::IoFormat_Native;
        case ma_format_f32: return Audio::IoFormat_F32;
        case ma_format_s32: return Audio::IoFormat_S32;
        case ma_format_s16: return Audio::IoFormat_S16;
        case ma_format_s24: return Audio::IoFormat_S24;
        case ma_format_u8: return Audio::IoFormat_U8;
        default: return Audio::IoFormat_Native;
    }
}

// #include "CDSPResampler.h"
// See https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation
// static unique_ptr<r8b::CDSPResampler24> Resampler;
// int resampled_frames = Resampler->process(read_ptr, available_resample_read_frames, resampled_buffer);
// Set up resampler if needed.
// if (InStream->sample_rate != OutStream->sample_rate) {
// Resampler = make_unique<r8b::CDSPResampler24>(InStream->sample_rate, OutStream->sample_rate, 1024); // todo can we get max frame size here?
// }

static dsp *FaustDsp = nullptr;

void FaustNodeProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    FaustDsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);
    for (Count bus = 0; bus < 1; bus++) {
        for (Count i = 0; i < *frame_count_out; i++) {
            if (s.Audio.MonitorInput) bus_frames_out[bus][i] += bus_frames_in[bus][i];
        }
    }

    (void)node; // unused
    (void)frame_count_in; // unused
}

static ma_node_graph NodeGraph;
static ma_audio_buffer_ref InputBuffer;

void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    ma_audio_buffer_ref_set_data(&InputBuffer, input, frame_count);
    ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

// Faust vars:
static llvm_dsp_factory *DspFactory;
static Box FaustBox = nullptr;
static unique_ptr<FaustParams> FaustUi;

static ma_node_graph_config NodeGraphConfig;
static ma_data_source_node InputNode;
static ma_data_source_node_config InputNodeConfig;
static ma_node_vtable FaustNodeVTable = {FaustNodeProcess, nullptr, 1, 1, 0};
static ma_node_config FaustNodeConfig;
static ma_node_base FaustNode;

// Context
static ma_context AudioContext;
static bool AudioContextInitialized = false;
static vector<ma_device_info *> DeviceInfos[IO_Count];
static vector<string> DeviceNames[IO_Count];
// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;

static vector<Audio::IoFormat> NativeFormats;
static vector<U32> NativeSampleRates;

// Current device
static ma_device_config DeviceConfig;
static ma_device Device;
static ma_device_info DeviceInfo;

// State value cache vars:
static string PreviousFaustCode;
static string PreviousInDeviceName, PreviousOutDeviceName;
static Audio::IoFormat PreviousInFormat, PreviousOutFormat;
static U32 PreviousSampleRate;
static bool PreviousFaustRunning;

static inline bool IsDeviceStarted() { return ma_device_is_started(&Device); }

static const ma_device_id *GetDeviceId(IO io, string_view device_name) {
    for (const ma_device_info *info : DeviceInfos[io]) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

const string Audio::GetFormatName(const Audio::IoFormat format) {
    const bool is_native = std::find(NativeFormats.begin(), NativeFormats.end(), format) != NativeFormats.end();
    return ::format("{}{}", ma_get_format_name(ToAudioFormat(format)), is_native ? "*" : "");
}
const string Audio::GetSampleRateName(const U32 sample_rate) {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return format("{}{}", to_string(sample_rate), is_native ? "*" : "");
}

void Audio::Init() const {
    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio context: {}", result));

    static Count PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio devices: {}", result));

    for (Count i = 0; i < CaptureDeviceCount; i++) {
        DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
    }
    for (Count i = 0; i < PlaybackDeviceCount; i++) {
        DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
        DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
    }

    // todo need to clarify that the cross-product of these formats & sample rates are supported natively, and not just each config jointly
    for (Count i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
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

void Audio::InitDevice() const {
    if (!AudioContextInitialized) Init(); // todo explicit re-scan action

    // MA graph nodes require f32 format for in/out.
    // We could keep IO formats configurable, and add two decoders to/from f32, but MA already does this
    // conversion from native formats (if needed) since we specify f32 format in the device config, so it
    // would just be needlessly wasting cycles/memory (memory since an extra input buffer would be needed).
    // todo option to change dither mode, only present when used
    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ma_format_f32;
    // DeviceConfig.capture.format = ToAudioFormat(InFormat);
    DeviceConfig.capture.channels = 1; // Temporary (2)
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ma_format_f32;
    // DeviceConfig.playback.format = ToAudioFormat(OutFormat);
    DeviceConfig.playback.channels = 1; // Temporary (2)
    DeviceConfig.dataCallback = DataCallback;
    DeviceConfig.sampleRate = SampleRate;

    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    int result = ma_device_init(nullptr, &DeviceConfig, &Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(Device.pContext, Device.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio device info: {}", result));

    NodeGraphConfig = ma_node_graph_config_init(Device.capture.channels);

    result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize node graph: {}", result));

    // Attach input node to bus 0 of the Faust node.
    result = ma_audio_buffer_ref_init(Device.capture.format, Device.capture.channels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize input audio buffer: ", result));

    InputNodeConfig = ma_data_source_node_config_init(&InputBuffer);
    result = ma_data_source_node_init(&NodeGraph, &InputNodeConfig, nullptr, &InputNode);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize input node: ", result));

    result = ma_device_start(&Device);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", result));
}

void Audio::TeardownDevice() const {
    ma_data_source_node_uninit(&InputNode, nullptr);
    ma_node_graph_uninit(&NodeGraph, nullptr);
    ma_device_uninit(&Device);
    // ma_resampler_uninit(&Resampler, nullptr);
}

// todo still need to call this on app shutdown.
void Audio::Teardown() const {
    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error shutting down audio context: {}", result));
    AudioContextInitialized = false;
}

void Audio::UpdateProcess() const {
    const auto &Faust = s.Faust;
    const bool device_started = IsDeviceStarted();
    const bool sample_rate_changed = PreviousSampleRate != SampleRate;
    if (Running && !device_started) {
        InitDevice();
    } else if (!Running && device_started) {
        TeardownDevice();
    } else if (
        device_started &&
        (PreviousInDeviceName != InDeviceName || PreviousOutDeviceName != OutDeviceName ||
         PreviousInFormat != InFormat || PreviousOutFormat != OutFormat ||
         sample_rate_changed)
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

    if (device_started) {
        ma_device_set_master_volume(&Device, OutDeviceVolume);
        ma_node_set_output_bus_volume(&FaustNode, 0, FaustVolume);
    }

    if (FaustRunning != PreviousFaustRunning || Faust.Code != PreviousFaustCode || sample_rate_changed) {
        PreviousFaustRunning = FaustRunning;
        PreviousFaustCode = string(Faust.Code);

        string error_msg;
        destroyLibContext();
        if (FaustRunning && Faust.Code && U32(SampleRate)) {
            createLibContext();

            int argc = 0;
            const char **argv = new const char *[8];
            argv[argc++] = "-I";
            argv[argc++] = fs::relative("../lib/faust/libraries").c_str();
            if (sizeof(Sample) == sizeof(double)) argv[argc++] = "-double";

            int num_inputs, num_outputs;
            FaustBox = DSPToBoxes("FlowGrid", Faust.Code, argc, argv, &num_inputs, &num_outputs, error_msg);
            if (FaustBox && error_msg.empty()) {
                static const int optimize_level = -1;
                DspFactory = createDSPFactoryFromBoxes("FlowGrid", FaustBox, argc, argv, "", error_msg, optimize_level);
            }
            if (!FaustBox && error_msg.empty()) error_msg = "`DSPToBoxes` returned no error but did not produce a result.";
        }

        if (FaustRunning && DspFactory && error_msg.empty()) {
            FaustDsp = DspFactory->createDSPInstance();
            if (!FaustDsp) error_msg = "Could not create Faust DSP.";
            else {
                FaustDsp->init(SampleRate);

                if (FaustDsp->getNumOutputs() > 0 || FaustDsp->getNumInputs() > 0) {
                    // Attach Faust node to the graph endpoint.
                    FaustNodeConfig = ma_node_config_init();
                    FaustNodeConfig.vtable = &FaustNodeVTable;
                    FaustNodeConfig.pInputChannels = (U32[]){U32(FaustDsp->getNumInputs())}; // One input bus, with N channels.
                    FaustNodeConfig.pOutputChannels = (U32[]){U32(FaustDsp->getNumOutputs())}; // One output bus, with M channels.

                    int result = ma_node_init(&NodeGraph, &FaustNodeConfig, nullptr, &FaustNode);
                    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize the Faust node: {}", result));

                    ma_node_attach_output_bus(&FaustNode, 0, ma_node_graph_get_endpoint(&NodeGraph), 0);
                    ma_node_attach_output_bus(&InputNode, 0, &FaustNode, 0);
                }

                FaustUi = make_unique<FaustParams>();
                FaustDsp->buildUserInterface(FaustUi.get());
            }
        } else {
            FaustBox = nullptr;
            FaustUi = nullptr;
            if (FaustDsp) {
                if (FaustDsp->getNumOutputs() > 0 || FaustDsp->getNumInputs() > 0) {
                    ma_node_uninit(&FaustNode, nullptr);
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
}

using namespace ImGui;

static void DrawDevice(ma_device *device) {
    assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

    Text("[%s]", ma_get_backend_name(device->pContext->backend));

    static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
    ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), nullptr);
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

    ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), nullptr);
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

// void ShowBufferPlots() {
//     for (IO io : IO_All) {
//         const bool is_in = io == IO_In;
//         if (TreeNode(Capitalize(to_string(io)).c_str())) {
//             const auto *area = is_in ? Areas[IO_In] : Areas[IO_Out];
//             if (!area) continue;

//             const auto *device = is_in ? InStream->device : OutStream->device;
//             const auto &layout = is_in ? InStream->layout : OutStream->layout;
//             const auto frame_count = is_in ? LastReadFrameCount : LastWriteFrameCount;
//             if (ImPlot::BeginPlot(device->name, {-1, 160})) {
//                 ImPlot::SetupAxes("Sample index", "Value");
//                 ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
//                 ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
//                 for (int channel_index = 0; channel_index < layout.channel_count; channel_index++) {
//                     const auto &channel = layout.channels[channel_index];
//                     const char *channel_name = soundio_get_channel_name(channel);
//                     // todo Adapt the pointer casting to the sample format.
//                     //  Also, this works but very scary and I can't even justify why this seems to work so well,
//                     //  since the area pointer position gets updated in the separate read/write callbacks.
//                     //  Hrm.. are the start points of each channel area static after initializing the stream?
//                     //  If so, could just set those once on stream init and use them here!
//                     ImPlot::PlotLine(channel_name, (Sample *)area[channel_index].ptr, frame_count);
//                 }
//                 ImPlot::EndPlot();
//             }
//             TreePop();
//         }
//     }
// }

void Audio::Render() const {
    Running.Draw();
    if (!IsDeviceStarted()) {
        TextUnformatted("No audio device started yet");
        return;
    }

    FaustRunning.Draw(); // todo destroy/create faust context based on this
    Muted.Draw();
    MonitorInput.Draw();
    OutDeviceVolume.Draw();
    FaustVolume.Draw();
    SampleRate.Render(PrioritizedSampleRates);

    for (const IO io : IO_All) {
        NewLine();
        TextUnformatted(Capitalize(to_string(io)).c_str());
        (io == IO_In ? InDeviceName : OutDeviceName).Render(DeviceNames[io]);
        // (io == IO_In ? InFormat : OutFormat).Render(PrioritizedFormats); // See above - always using f32 format.
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
}
