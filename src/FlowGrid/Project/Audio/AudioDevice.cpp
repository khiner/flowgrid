#include "AudioDevice.h"

#include "miniaudio.h"

#include "imgui.h"

#include "Helper/String.h"

// Copied from `miniaudio.c::g_maStandardSampleRatePriorities`.
static ma_uint32 StandardSampleRatePriorities[] = {
    (ma_uint32)ma_standard_sample_rate_48000,
    (ma_uint32)ma_standard_sample_rate_44100,

    (ma_uint32)ma_standard_sample_rate_32000,
    (ma_uint32)ma_standard_sample_rate_24000,
    (ma_uint32)ma_standard_sample_rate_22050,

    (ma_uint32)ma_standard_sample_rate_88200,
    (ma_uint32)ma_standard_sample_rate_96000,
    (ma_uint32)ma_standard_sample_rate_176400,
    (ma_uint32)ma_standard_sample_rate_192000,

    (ma_uint32)ma_standard_sample_rate_16000,
    (ma_uint32)ma_standard_sample_rate_11025,
    (ma_uint32)ma_standard_sample_rate_8000,

    (ma_uint32)ma_standard_sample_rate_352800,
    (ma_uint32)ma_standard_sample_rate_384000,
};

const std::vector<U32> AudioDevice::PrioritizedSampleRates = {std::begin(StandardSampleRatePriorities), std::end(StandardSampleRatePriorities)};

static std::vector<ma_format> NativeFormats;
static std::vector<U32> NativeSampleRates;

static ma_context AudioContext;
static ma_device MaDevice;
static ma_device_info DeviceInfo;

AudioDevice::AudioDevice(ComponentArgs &&args, AudioDevice::AudioCallback callback) : Component(std::move(args)), Callback(callback) {
    Init();

    const Field::References listened_fields{On, InDeviceName, OutDeviceName, InFormat, OutFormat, InChannels, OutChannels, SampleRate};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
}

AudioDevice::~AudioDevice() {
    Field::UnregisterChangeListener(this);
    Uninit();
}

void AudioDevice::OnFieldChanged() {
    if (On.IsChanged() ||
        InDeviceName.IsChanged() ||
        OutDeviceName.IsChanged() ||
        InFormat.IsChanged() ||
        OutFormat.IsChanged() ||
        InChannels.IsChanged() ||
        OutChannels.IsChanged() ||
        SampleRate.IsChanged()) {
        const bool is_started = IsStarted();
        if (On && !is_started) {
            Init();
        } else if (!On && is_started) {
            Uninit();
        } else if (is_started) {
            // todo no need to completely reset in some cases (like when only format has changed) - just modify as needed in `Device::Update`.
            // todo sample rate conversion is happening even when choosing a SR that is native to both input & output, if it's not the highest-priority SR.
            Uninit();
            Init();
        }
    }
}

const string AudioDevice::GetFormatName(const int format) {
    const bool is_native = std::find(NativeFormats.begin(), NativeFormats.end(), format) != NativeFormats.end();
    return ::std::format("{}{}", ma_get_format_name((ma_format)format), is_native ? "*" : "");
}
const string AudioDevice::GetSampleRateName(const U32 sample_rate) {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return std::format("{}{}", to_string(sample_rate), is_native ? "*" : "");
}

static std::vector<ma_device_info *> DeviceInfos[IO_Count];
static std::vector<string> DeviceNames[IO_Count];
static const ma_device_id *GetDeviceId(IO io, string_view device_name) {
    for (const ma_device_info *info : DeviceInfos[io]) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

ma_device *AudioDevice::Get() const { return &MaDevice; }

void AudioDevice::Init() {
    int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio context: {}", result));

    static Count PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", result));

    for (Count i = 0; i < CaptureDeviceCount; i++) {
        DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
    }
    for (Count i = 0; i < PlaybackDeviceCount; i++) {
        DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
        DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
    }

    ma_device_config config;
    config = ma_device_config_init(ma_device_type_duplex);
    config.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    config.capture.format = ma_format_f32;
    config.capture.channels = InChannels;
    config.capture.shareMode = ma_share_mode_shared;
    config.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    config.playback.format = ma_format_f32;
    config.playback.channels = OutChannels;
    config.dataCallback = Callback;
    config.sampleRate = SampleRate;
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer already writes to every output frame.

    // MA graph nodes require f32 format for in/out.
    // We could keep IO formats configurable, and add two decoders to/from f32, but MA already does this
    // conversion from native formats (if needed) since we specify f32 format in the device config, so it
    // would just be needlessly wasting cycles/memory (memory since an extra input buffer would be needed).
    // todo option to change dither mode, only present when used
    // config.capture.format = ToAudioFormat(InFormat);
    // config.playback.format = ToAudioFormat(OutFormat);

    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    result = ma_device_init(nullptr, &config, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(MaDevice.pContext, MaDevice.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio device info: {}", result));

    // todo need to verify that the cross-product of these formats & sample rates are supported natively, and not just each config jointly
    for (Count i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
    }

    if (MaDevice.capture.name != InDeviceName) InDeviceName.Set_(MaDevice.capture.name);
    if (MaDevice.playback.name != OutDeviceName) OutDeviceName.Set_(MaDevice.playback.name);
    if (MaDevice.capture.format != InFormat) InFormat.Set_(MaDevice.capture.format);
    if (MaDevice.playback.format != OutFormat) OutFormat.Set_(MaDevice.playback.format);
    if (MaDevice.capture.channels != InChannels) InChannels.Set_(MaDevice.capture.channels);
    if (MaDevice.playback.channels != OutChannels) OutChannels.Set_(MaDevice.playback.channels);
    if (MaDevice.sampleRate != SampleRate) SampleRate.Set_(MaDevice.sampleRate);

    result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio device: {}", result));
}

void AudioDevice::Uninit() {
    if (IsStarted()) {
        const int result = ma_device_stop(&MaDevice);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error stopping audio device: {}", result));
    }

    ma_device_uninit(&MaDevice);
    // ma_resampler_uninit(&Resampler, nullptr);

    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error shutting down audio context: {}", result));
}

bool AudioDevice::IsStarted() const { return ma_device_is_started(&MaDevice); }

using namespace ImGui;

void AudioDevice::Render() const {
    On.Draw();
    if (!IsStarted()) {
        TextUnformatted("Audio device is not started.");
        return;
    }
    SampleRate.Render(PrioritizedSampleRates);
    for (const IO io : IO_All) {
        TextUnformatted(StringHelper::Capitalize(to_string(io)).c_str());
        (io == IO_In ? InDeviceName : OutDeviceName).Render(DeviceNames[io]);
        // (io == IO_In ? InFormat : OutFormat).Render(PrioritizedFormats); // See above - always using f32 format.
    }
    if (TreeNode("Info")) {
        auto *device = &MaDevice;
        assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

        Text("[%s]", ma_get_backend_name(device->pContext->backend));

        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), nullptr);
        if (TreeNode(std::format("{} ({})", name, "Capture").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->capture.internalFormat), ma_get_format_name(device->capture.format));
            Text("Channels: %d -> %d", device->capture.internalChannels, device->capture.channels);
            Text("Sample Rate: %d -> %d", device->capture.internalSampleRate, device->sampleRate);
            Text("Buffer Size: %d*%d (%d)\n", device->capture.internalPeriodSizeInFrames, device->capture.internalPeriods, (device->capture.internalPeriodSizeInFrames * device->capture.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (TreeNode(std::format("{} ({})", name, "Playback").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->playback.format), ma_get_format_name(device->playback.internalFormat));
            Text("Channels: %d -> %d", device->playback.channels, device->playback.internalChannels);
            Text("Sample Rate: %d -> %d", device->sampleRate, device->playback.internalSampleRate);
            Text("Buffer Size: %d*%d (%d)", device->playback.internalPeriodSizeInFrames, device->playback.internalPeriods, (device->playback.internalPeriodSizeInFrames * device->playback.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        TreePop();
    }
}

// todo implement for r8brain resampler
// todo I want to use this currently to support quality/fast resampling between _natively supported_ device sample rates.
//   Can I still use duplex mode in this case?
// #include "CDSPResampler.h"
// See https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation
// static unique_ptr<r8b::CDSPResampler24> Resampler;
// int resampled_frames = Resampler->process(read_ptr, available_resample_read_frames, resampled_buffer);
// Set up resampler if needed.
// if (InStream->sample_rate != OutStream->sample_rate) {
// Resampler = make_unique<r8b::CDSPResampler24>(InStream->sample_rate, OutStream->sample_rate, 1024); // todo can we get max frame size here?
// }
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
