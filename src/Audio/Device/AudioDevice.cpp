#include "AudioDevice.h"

#include <algorithm>
#include <format>
#include <ranges>

#include "imgui.h"

using std::string, std::string_view;
using std::ranges::any_of, std::ranges::find_if;

// Copied from `miniaudio.c::g_maStandardSampleRatePriorities`.
const std::vector<u32> AudioDevice::PrioritizedSampleRates = {
    ma_standard_sample_rate_48000,
    ma_standard_sample_rate_44100,

    ma_standard_sample_rate_32000,
    ma_standard_sample_rate_24000,
    ma_standard_sample_rate_22050,

    ma_standard_sample_rate_88200,
    ma_standard_sample_rate_96000,
    ma_standard_sample_rate_176400,
    ma_standard_sample_rate_192000,

    ma_standard_sample_rate_16000,
    ma_standard_sample_rate_11025,
    ma_standard_sample_rate_8000,

    ma_standard_sample_rate_352800,
    ma_standard_sample_rate_384000,
};

struct Context {
    Context() {
        if (ma_result result = ma_context_init(nullptr, 0, nullptr, &MaContext); result != MA_SUCCESS) {
            throw std::runtime_error(std::format("Error initializing audio context: {}", int(result)));
        }
        ScanDevices();
    }
    ~Context() {
        ma_context_uninit(&MaContext);
    }

    const ma_device_info *GetDeviceInfo(IO type, string_view name) const {
        for (const auto *info : DeviceInfos[type]) {
            if ((name.empty() && info->isDefault) || info->name == name) return info;
        }
        return nullptr;
    }

    std::optional<DeviceDataFormat> FindFormatWithNativeSampleRate(IO type, u32 sample_rate) const {
        const auto &native_data_formats = NativeDataFormats[type];
        auto it = find_if(native_data_formats, [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
        if (it != native_data_formats.end()) return *it;

        return {};
    }

    bool IsNativeSampleRate(IO type, u32 sample_rate) const {
        return any_of(NativeDataFormats[type], [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
    }

    DeviceDataFormat FindNativeFormatWithNearestSampleRate(IO type, u32 target) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        return *std::min_element(NativeDataFormats[type].begin(), NativeDataFormats[type].end(), [target](const DeviceDataFormat a, const DeviceDataFormat b) {
            auto diff_a = std::abs(s64(a.SampleRate) - target);
            auto diff_b = std::abs(s64(b.SampleRate) - target);
            return diff_a < diff_b || (diff_a == diff_b && a.SampleRate > b.SampleRate); // Favor higher sample rates if there is a tie.
        });
    }

    // If no target format is provided, returns the the native format with the highest-priority sample rate.
    // Otherwise, returns the target format if it is natively supported, or the native format with the nearest native sample rate otherwise.
    // todo channels
    DeviceDataFormat GetHighestPriorityNativeFormat(IO type, std::optional<DeviceDataFormat> target_native_format) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        if (!target_native_format || target_native_format->SampleRate == 0) {
            // No target requested. Choose the native format with the highest-priority sample rate.
            for (u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
                if (auto format = FindFormatWithNativeSampleRate(type, sample_rate)) return *format;
            }

            // The device doesn't natively support any of the prioritized sample rates. Return the first native format.
            return NativeDataFormats[type][0];
        }

        // Specific sample rate requested.
        if (auto native_format = FindFormatWithNativeSampleRate(type, target_native_format->SampleRate)) return *native_format;

        // A specific sample rate was requested that's not natively supported. Return the native format with the nearest sample rate.
        return FindNativeFormatWithNearestSampleRate(type, target_native_format->SampleRate);
    }

    void ScanDevices() {
        static u32 PlaybackDeviceCount, CaptureDeviceCount;
        static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
        ma_result result = ma_context_get_devices(&MaContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", int(result)));

        for (IO io : IO_All) DeviceInfos[io].clear();
        for (u32 i = 0; i < CaptureDeviceCount; i++) DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        for (u32 i = 0; i < PlaybackDeviceCount; i++) DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);

        for (IO io : IO_All) {
            NativeDataFormats[io].clear();

            ma_device_info device_info;
            if (result = ma_context_get_device_info(&MaContext, io == IO_In ? ma_device_type_capture : ma_device_type_playback, nullptr, &device_info);
                result != MA_SUCCESS) {
                throw std::runtime_error(std::format("Error getting audio {} device info: {}", to_string(io), int(result)));
            }

            for (u32 i = 0; i < device_info.nativeDataFormatCount; i++) {
                const auto &df = device_info.nativeDataFormats[i];
                NativeDataFormats[io].emplace_back(df.format, df.channels, df.sampleRate);
            }
        }
    }

    ma_context MaContext;
    std::vector<const ma_device_info *> DeviceInfos[IO_Count];
    std::vector<DeviceDataFormat> NativeDataFormats[IO_Count];
};

static std::unique_ptr<Context> AudioContext;
static u32 DeviceInstanceCount = 0; // Reference count for the audio context. When this goes from nonzero to zero, the context is destroyed.

AudioDevice::Config::Config(IO type, TargetConfig &&target) {
    if (!AudioContext) AudioContext = std::make_unique<Context>();

    NativeFormat = AudioContext->GetHighestPriorityNativeFormat(type, target.NativeFormat ? target.NativeFormat : target.ClientFormat);

    ClientFormat = {
        target.ClientFormat && target.ClientFormat->SampleFormat != ma_format_unknown ? target.ClientFormat->SampleFormat : NativeFormat.SampleFormat,
        target.ClientFormat && target.ClientFormat->Channels != 0 ? target.ClientFormat->Channels : NativeFormat.Channels,
        target.ClientFormat && target.ClientFormat->SampleRate != 0 ? target.ClientFormat->SampleRate : NativeFormat.SampleRate,
    };

    DeviceName = "";
    for (const ma_device_info *info : AudioContext->DeviceInfos[type]) {
        if (!target.DeviceName.empty() && !info->isDefault && info->name == target.DeviceName) {
            DeviceName = info->name;
            break;
        }
    }
}

AudioDevice::AudioDevice(IO type, AudioDevice::AudioCallback callback, TargetConfig &&target_config, const void *client_user_data)
    : Type(std::move(type)), Callback(std::move(callback)), _UserData({this, client_user_data}), _Config(Type, std::move(target_config)) {
    Init();
    DeviceInstanceCount++;
}

AudioDevice::~AudioDevice() {
    Uninit();

    if (--DeviceInstanceCount == 0) AudioContext.reset();
}

void AudioDevice::SetConfig(TargetConfig &&target_config) {
    // Only reinitialize if the computed config is different than current one.
    const Config new_config{Type, std::move(target_config)};
    if (new_config == _Config) return;

    _Config = std::move(new_config);
    if (Device) Uninit();
    Init();
}

void AudioDevice::Init() {
    Device = std::make_unique<ma_device>();

    const auto ma_type = IsInput() ? ma_device_type_capture : ma_device_type_playback;
    auto ma_config = ma_device_config_init(ma_type);

    const ma_device_id *device_id = nullptr;
    for (const ma_device_info *info : AudioContext->DeviceInfos[Type]) {
        if (!_Config.DeviceName.empty() && !info->isDefault && info->name == _Config.DeviceName) {
            device_id = &(info->id);
            break;
        }
    }

    if (IsInput()) {
        ma_config.capture.pDeviceID = device_id;
        ma_config.capture.format = ma_format(_Config.ClientFormat.SampleFormat);
        ma_config.capture.channels = _Config.ClientFormat.Channels;
        // `noFixedSizedCallback` is more efficient, but don't be tempted.
        // It works fine until a manual input device change, which breaks things in inconsistent ways until we
        // disconnect and reconnect the input device node.
        // One way out of this would be to do just that - have device nodes listen for device re-inits and
        // send an `OnNodeConnectionsChanged` to the parent graph.
        // I think this would work fine, but the fact that it works smoothly without any connection resets seems
        // better for this stage (favoring stability over performance in general).
        // Also, enabling this flag this seems to work fine for the output device as well, with the same caveats.
        // config.noFixedSizedCallback = true;
    } else {
        ma_config.playback.pDeviceID = device_id;
        ma_config.playback.format = ma_format(_Config.ClientFormat.SampleFormat);
        ma_config.playback.channels = _Config.ClientFormat.Channels;
    }

    ma_config.dataCallback = Callback;
    ma_config.pUserData = &_UserData;
    ma_config.sampleRate = _Config.ClientFormat.SampleRate;

    const u32 from_sample_rate = IsInput() ? _Config.NativeFormat.SampleRate : _Config.ClientFormat.SampleRate;
    const u32 to_sample_rate = IsInput() ? _Config.ClientFormat.SampleRate : _Config.NativeFormat.SampleRate;
    // Resampler format/channels aren't used.
    ma_config.resampling = ma_resampler_config_init(ma_format_unknown, 0, from_sample_rate, to_sample_rate, ma_resample_algorithm_linear);

    ma_config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer writes to every output frame.
    ma_config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.

    ma_result result = ma_device_init(nullptr, &ma_config, Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(Type), int(result)));

    result = ma_device_get_info(Device.get(), ma_type, &Info);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting device info: {}", int(result)));

    Device->onNotification = [](const ma_device_notification *notification) {
        switch (notification->type) {
            case ma_device_notification_type_started:
            case ma_device_notification_type_stopped:
                break;
            case ma_device_notification_type_rerouted:
                // A reroute happens when the default device is changed, which happens when we initialize a default MA device,
                // and e.g. a new audio device is plugged in.
                // Note that we don't change the device name here, since this is only triggered for the default device,
                // and we set `Name` to an empty string for _any_ default device.
                AudioContext->ScanDevices();
                break;
            case ma_device_notification_type_interruption_began:
            case ma_device_notification_type_interruption_ended:
            case ma_device_notification_type_unlocked:
                break;
        }
    };

    result = ma_device_start(Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio {} device: {}", to_string(Type), int(result)));

    // todo option to change dither mode, only present when used
    // todo implement for r8brain resampler
    // static ma_resampler_config ResamplerConfig;
    // static ma_resampler Resampler;
    // #include "CDSPResampler.h"
    // See https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation
    // static unique_ptr<r8b::CDSPResampler24> Resampler;
    // int resampled_frames = Resampler->process(read_ptr, available_resample_read_frames, resampled_buffer);
    // Set up resampler if needed.
    // if (in_sample_rate != out_sample_rate) {
    // Resampler = make_unique<r8b::CDSPResampler24>(in_sample_rate, out_sample_rate, 1024); // todo can we get max frame size here?
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
    //
    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;
}

void AudioDevice::Uninit() {
    Stop();
    ma_device_uninit(Device.get());
    Device.reset();
}

void AudioDevice::Stop() {
    if (IsStarted()) ma_device_stop(Device.get());
}

void AudioDevice::ScanDevices() { AudioContext->ScanDevices(); }
std::string AudioDevice::GetName() const { return Info.name; }
bool AudioDevice::IsDefault() const { return Info.isDefault; }

const std::vector<DeviceDataFormat> &AudioDevice::GetNativeFormats() const { return AudioContext->NativeDataFormats[Type]; }
const std::vector<const ma_device_info *> &AudioDevice::GetAllInfos() const { return AudioContext->DeviceInfos[Type]; }

ma_format AudioDevice::GetNativeSampleFormat() const {
    if (!Device) return ma_format_f32;
    if (IsInput()) return Device->capture.internalFormat;
    return Device->playback.internalFormat;
}

u32 AudioDevice::GetNativeSampleRate() const {
    if (!Device) return 0;
    if (IsInput()) return Device->capture.internalSampleRate;
    return Device->playback.internalSampleRate;
}

u32 AudioDevice::GetNativeChannels() const {
    if (!Device) return 0;
    if (IsInput()) return Device->capture.internalChannels;
    return Device->playback.internalChannels;
}

u32 AudioDevice::GetBufferFrames() const {
    if (!Device) return 0;
    // if (IsInput()) return Device->capture.internalPeriodSizeInFrames * Device->capture.internalPeriods;
    // return Device->playback.internalPeriodSizeInFrames * Device->playback.internalPeriods;
    if (IsInput()) return Device->capture.internalPeriodSizeInFrames;
    return Device->playback.internalPeriodSizeInFrames;
}

DeviceDataFormat AudioDevice::GetNativeFormat() const { return {GetNativeSampleFormat(), GetNativeChannels(), GetNativeSampleRate()}; }

bool AudioDevice::IsNativeSampleRate(u32 sample_rate) const { return AudioContext->IsNativeSampleRate(Type, sample_rate); }

bool AudioDevice::IsStarted() const { return ma_device_is_started(Device.get()); }

using namespace ImGui;

void AudioDevice::RenderInfo() const {
    auto *device = Device.get();
    Text("%s (%s)", GetName().c_str(), IsInput() ? "Capture" : "Playback");
    Text("Backend: %s", ma_get_backend_name(device->pContext->backend));
    if (IsInput()) {
        Text("Format: %s -> %s", DeviceDataFormat::GetFormatName(device->capture.internalFormat), DeviceDataFormat::GetFormatName(device->capture.format));
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
    } else {
        Text("Format: %s -> %s", DeviceDataFormat::GetFormatName(device->playback.format), DeviceDataFormat::GetFormatName(device->playback.internalFormat));
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
    }
}
