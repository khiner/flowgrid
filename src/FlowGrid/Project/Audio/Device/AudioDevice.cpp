#include "AudioDevice.h"

#include <format>
#include <range/v3/range/conversion.hpp>
#include <ranges>

#include "imgui.h"

using std::string;
using std::string_view;

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
        ma_result result = ma_context_init(nullptr, 0, nullptr, &MaContext);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio context: {}", int(result)));

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
        auto it = std::find_if(native_data_formats.begin(), native_data_formats.end(), [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
        if (it != native_data_formats.end()) return *it;

        return {};
    }

    bool IsNativeSampleRate(IO type, u32 sample_rate) const {
        const auto &native_data_formats = NativeDataFormats[type];
        return std::any_of(native_data_formats.begin(), native_data_formats.end(), [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
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
    DeviceDataFormat GetHighestPriorityNativeFormat(IO type, std::optional<DeviceDataFormat> native_format_target) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        if (!native_format_target || native_format_target->SampleRate == 0) {
            // No target requested. Choose the native format with the highest-priority sample rate.
            for (u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
                if (auto format = FindFormatWithNativeSampleRate(type, sample_rate)) return *format;
            }

            // The device doesn't natively support any of the prioritized sample rates. Return the first native format.
            return NativeDataFormats[type][0];
        }

        // Specific sample rate requested.
        if (IsNativeSampleRate(type, native_format_target->SampleRate)) return *native_format_target;

        // A specific sample rate was requested that's not natively supported. Return the native format with the nearest sample rate.
        return FindNativeFormatWithNearestSampleRate(type, native_format_target->SampleRate);
    }

    void ScanDevices() {
        static u32 PlaybackDeviceCount, CaptureDeviceCount;
        static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
        ma_result result = ma_context_get_devices(&MaContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", int(result)));

        for (IO io : IO_All) DeviceInfos[io].clear();
        for (u32 i = 0; i < CaptureDeviceCount; i++) DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        for (u32 i = 0; i < PlaybackDeviceCount; i++) DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);

        for (const IO io : IO_All) {
            NativeDataFormats[io].clear();

            ma_device_info device_info;
            result = ma_context_get_device_info(&MaContext, io == IO_In ? ma_device_type_capture : ma_device_type_playback, nullptr, &device_info);
            if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio {} device info: {}", to_string(io), int(result)));

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

AudioDevice::AudioDevice(IO type, AudioDevice::AudioCallback callback, std::optional<DeviceDataFormat> client_format, std::optional<DeviceDataFormat> native_format_target, string_view device_name_target, void *client_user_data)
    : Type(type), Callback(callback), _UserData({this, client_user_data}) {
    if (!AudioContext) AudioContext = std::make_unique<Context>();
    Init(std::move(client_format), std::move(native_format_target), device_name_target);
    DeviceInstanceCount++;
}

AudioDevice::~AudioDevice() {
    Uninit();

    DeviceInstanceCount--;
    if (DeviceInstanceCount == 0) AudioContext.reset();
}

void AudioDevice::Init(std::optional<DeviceDataFormat> client_format, std::optional<DeviceDataFormat> native_format_target, string_view device_name_target) {
    Device = std::make_unique<ma_device>();

    const ma_device_id *device_id = nullptr;
    for (const ma_device_info *info : AudioContext->DeviceInfos[Type]) {
        if (!device_name_target.empty() && !info->isDefault && info->name == device_name_target) {
            device_id = &(info->id);
            break;
        }
    }

    const ma_device_type ma_type = IsInput() ? ma_device_type_capture : ma_device_type_playback;
    ma_device_config config = ma_device_config_init(ma_type);
    if (IsInput()) {
        config.capture.pDeviceID = device_id;
        config.capture.format = ma_format_f32;
        config.capture.channels = client_format ? client_format->Channels : 0;
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
        config.playback.pDeviceID = device_id;
        config.playback.format = ma_format_f32;
        config.playback.channels = client_format ? client_format->Channels : 0;
    }

    config.dataCallback = Callback;
    config.pUserData = &_UserData;

    const auto native_format = AudioContext->GetHighestPriorityNativeFormat(Type, native_format_target ? native_format_target : client_format);

    // Store the fully-specified client format the device will be converting to/from.
    ClientFormat = {
        client_format && client_format->SampleFormat != ma_format_unknown ? client_format->SampleFormat : native_format.SampleFormat,
        client_format && client_format->Channels != 0 ? client_format->Channels : native_format.Channels,
        client_format && client_format->SampleRate != 0 ? client_format->SampleRate : native_format.SampleRate,
    };

    const u32 from_sample_rate = IsInput() ? native_format.SampleRate : ClientFormat.SampleRate;
    const u32 to_sample_rate = IsInput() ? ClientFormat.SampleRate : native_format.SampleRate;

    config.sampleRate = ClientFormat.SampleRate;
    // Resampler format/channels aren't used.
    config.resampling = ma_resampler_config_init(ma_format_unknown, 0, from_sample_rate, to_sample_rate, ma_resample_algorithm_linear);
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer writes to every output frame.
    config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.

    ma_result result = ma_device_init(nullptr, &config, Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(Type), int(result)));

    result = ma_device_get_info(Device.get(), ma_type, &Info);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting device info: {}", int(result)));

    Device->onNotification = [](const ma_device_notification *notification) {
        switch (notification->type) {
            case ma_device_notification_type_started:
                break;
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
                break;
            case ma_device_notification_type_interruption_ended:
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
    if (IsStarted()) ma_device_stop(Device.get());
    ma_device_uninit(Device.get());
    Device.reset();
}

void AudioDevice::ScanDevices() { AudioContext->ScanDevices(); }
std::string AudioDevice::GetName() const { return Info.name; }
bool AudioDevice::IsDefault() const { return Info.isDefault; }

const std::vector<DeviceDataFormat> &AudioDevice::GetNativeFormats() const { return AudioContext->NativeDataFormats[Type]; }
const std::vector<const ma_device_info *> &AudioDevice::GetAllInfos() const { return AudioContext->DeviceInfos[Type]; }

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

int AudioDevice::GetNativeSampleFormat() const {
    if (!Device) return ma_format_f32;
    if (IsInput()) return Device->capture.internalFormat;
    return Device->playback.internalFormat;
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
