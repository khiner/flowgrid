#include "AudioDevice.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "imgui.h"

#include "Core/Store/StoreAction.h"
#include "Helper/String.h"

const std::vector<U32> AudioDevice::PrioritizedSampleRates = {std::begin(g_maStandardSampleRatePriorities), std::end(g_maStandardSampleRatePriorities)};

static std::vector<ma_format> NativeFormats;
static std::vector<U32> NativeSampleRates;

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

static ma_context AudioContext;
static ma_device MaDevice;
static ma_device_config DeviceConfig;
static ma_device_info DeviceInfo;

void AudioDevice::Init(AudioDevice::Callback callback) const {
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

    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = InChannels;
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ma_format_f32;
    DeviceConfig.playback.channels = OutChannels;
    DeviceConfig.dataCallback = callback;
    DeviceConfig.sampleRate = SampleRate;

    // MA graph nodes require f32 format for in/out.
    // We could keep IO formats configurable, and add two decoders to/from f32, but MA already does this
    // conversion from native formats (if needed) since we specify f32 format in the device config, so it
    // would just be needlessly wasting cycles/memory (memory since an extra input buffer would be needed).
    // todo option to change dither mode, only present when used
    // DeviceConfig.capture.format = ToAudioFormat(InFormat);
    // DeviceConfig.playback.format = ToAudioFormat(OutFormat);

    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    result = ma_device_init(nullptr, &DeviceConfig, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(MaDevice.pContext, MaDevice.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio device info: {}", result));

    // todo need to clarify that the cross-product of these formats & sample rates are supported natively, and not just each config jointly
    for (Count i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
    }

    StoreEntries initial_settings;
    if (MaDevice.capture.name != InDeviceName) initial_settings.emplace_back(InDeviceName.Path, MaDevice.capture.name);
    if (MaDevice.playback.name != OutDeviceName) initial_settings.emplace_back(OutDeviceName.Path, MaDevice.playback.name);
    if (MaDevice.capture.format != InFormat) initial_settings.emplace_back(InFormat.Path, MaDevice.capture.format);
    if (MaDevice.playback.format != OutFormat) initial_settings.emplace_back(OutFormat.Path, MaDevice.playback.format);
    if (MaDevice.capture.channels != InChannels) initial_settings.emplace_back(InChannels.Path, MaDevice.capture.channels);
    if (MaDevice.playback.channels != OutChannels) initial_settings.emplace_back(OutChannels.Path, MaDevice.playback.channels);
    if (MaDevice.sampleRate != SampleRate) initial_settings.emplace_back(SampleRate.Path, MaDevice.sampleRate);
    if (!initial_settings.empty()) Action::SetValues{initial_settings}.q(true);
}

void AudioDevice::Update() const {
    if (IsStarted()) ma_device_set_master_volume(&MaDevice, Volume);
}

void AudioDevice::Uninit() const {
    ma_device_uninit(&MaDevice);
    // ma_resampler_uninit(&Resampler, nullptr);

    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error shutting down audio context: {}", result));
}

void AudioDevice::Start() const {
    if (IsStarted()) return;
    const int result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio device: {}", result));
}
void AudioDevice::Stop() const {
    if (!IsStarted()) return;
    const int result = ma_device_stop(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error stopping audio device: {}", result));
}
bool AudioDevice::IsStarted() const { return ma_device_is_started(&MaDevice); }

using namespace ImGui;

void AudioDevice::Render() const {
    On.Draw();
    if (!IsStarted()) {
        TextUnformatted("No audio device started yet");
        return;
    }
    Muted.Draw();
    SameLine();
    Volume.Draw();
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
