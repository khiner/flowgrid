#include "AudioDevice.h"

#include "imgui.h"
#include "miniaudio.h"

#include "Helper/String.h"

// Copied from `miniaudio.c::g_maStandardSampleRatePriorities`.
static const std::vector<u32> PrioritizedSampleRates = {
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

static std::unordered_map<IO, std::vector<ma_format>> NativeFormats;
static std::unordered_map<IO, std::vector<u32>> NativeSampleRates;

static ma_context AudioContext;
static u16 AudioContextInitializedCount = 0;

static std::vector<ma_device_info *> DeviceInfos[IO_Count];
static std::vector<string> DeviceNames[IO_Count];

AudioDevice::AudioDevice(ComponentArgs &&args, IO type, AudioDevice::AudioCallback callback, UserData user_data)
    : Component(std::move(args)), Type(type), Callback(callback), _UserData(user_data) {
    const Field::References listened_fields{On, Name, Format, Channels, SampleRate};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);

    Device = std::make_unique<ma_device>();

    AudioContextInitializedCount++;
    if (AudioContextInitializedCount <= 1) {
        int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio context: {}", result));

        static u32 PlaybackDeviceCount, CaptureDeviceCount;
        static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
        result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", result));

        for (u32 i = 0; i < CaptureDeviceCount; i++) {
            DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
            DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
        }
        for (u32 i = 0; i < PlaybackDeviceCount; i++) {
            DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
            DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
        }

        for (const IO io : IO_All) {
            ma_device_info DeviceInfo;

            result = ma_context_get_device_info(&AudioContext, io == IO_In ? ma_device_type_capture : ma_device_type_playback, nullptr, &DeviceInfo);
            if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio {} device info: {}", to_string(io), result));

            // todo need to verify that the cross-product of these formats & sample rates are supported natively.
            // Create a new format type that mirrors MA's (with sample format and sample rate).
            for (u32 i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
                const auto &native_format = DeviceInfo.nativeDataFormats[i];
                NativeFormats[io].emplace_back(native_format.format);
                NativeSampleRates[io].emplace_back(native_format.sampleRate);
            }
        }

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
    }

    ma_device_config config = ma_device_config_init(Type == IO_In ? ma_device_type_capture : ma_device_type_playback);

    const ma_device_id *device_id = nullptr;
    for (const ma_device_info *info : DeviceInfos[Type]) {
        if (info->name == string_view(Name)) {
            device_id = &(info->id);
            break;
        }
    }

    if (Type == IO_In) {
        config.capture.pDeviceID = device_id;
        config.capture.format = ma_format_f32;
        config.capture.channels = Channels;
    } else {
        config.playback.pDeviceID = device_id;
        config.playback.format = ma_format_f32;
        config.playback.channels = Channels;
    }

    config.dataCallback = Callback;
    config.pUserData = _UserData;
    config.sampleRate = GetConfigSampleRate();
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer already writes to every output frame.
    config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.
    int result = ma_device_init(nullptr, &config, Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(Type), result));

    // The device may have a different configuration than what we requested.
    // Update the fields to reflect the actual device configuration.
    if (Device->sampleRate != SampleRate) SampleRate.Set_(Device->sampleRate);
    if (Type == IO_Out) {
        if (Device->playback.name != Name) Name.Set_(Device->playback.name);
        if (Device->playback.format != Format) Format.Set_(Device->playback.format);
        if (Device->playback.channels != Channels) Channels.Set_(Device->playback.channels);
    } else {
        if (Device->capture.name != Name) Name.Set_(Device->capture.name);
        if (Device->capture.format != Format) Format.Set_(Device->capture.format);
        if (Device->capture.channels != Channels) Channels.Set_(Device->capture.channels);
    }
    result = ma_device_start(Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio {} device: {}", to_string(Type), result));
}

AudioDevice::~AudioDevice() {
    if (IsStarted()) ma_device_stop(Device.get());
    ma_device_uninit(Device.get());

    AudioContextInitializedCount--;
    if (AudioContextInitializedCount <= 0) {
        for (const IO io : IO_All) {
            DeviceInfos[io].clear();
            DeviceNames[io].clear();
        }
        ma_context_uninit(&AudioContext);
    }
    Field::UnregisterChangeListener(this);
}

void AudioDevice::OnFieldChanged() {
    if (On.IsChanged() || Name.IsChanged() || Format.IsChanged() || Channels.IsChanged() || SampleRate.IsChanged()) {
        // const bool is_started = IsStarted();
        // if (On && !is_started) {
        //     Init();
        // } else if (is_started) {
        //    if (!On) Uninit();
        //    Init();
        // }
    }
}

static bool IsNativeSampleRate(u32 sample_rate, IO type) {
    if (!NativeSampleRates.contains(type)) return false;

    const auto &native_sample_rates = NativeSampleRates.at(type);
    return std::find(native_sample_rates.begin(), native_sample_rates.end(), sample_rate) != native_sample_rates.end();
}

static bool IsNativeFormat(ma_format format, IO type) {
    if (!NativeFormats.contains(type)) return false;

    const auto &native_formats = NativeFormats.at(type);
    return std::find(native_formats.begin(), native_formats.end(), format) != native_formats.end();
}

string AudioDevice::GetFormatName(int format) const {
    return ::std::format("{}{}", ma_get_format_name(ma_format(format)), IsNativeFormat(ma_format(format), Type) ? "*" : "");
}
string AudioDevice::GetSampleRateName(u32 sample_rate) const {
    return std::format("{}{}", to_string(sample_rate), IsNativeSampleRate(sample_rate, Type) ? "*" : "");
}
u64 AudioDevice::GetBufferSize() const {
    return Type == IO_Out ? Device->playback.internalPeriodSizeInFrames : 0;
}

u32 AudioDevice::GetConfigSampleRate() const {
    // The native sample rate list may have a different prioritized order than our priority list.
    // We want to choose the highest-priority sample rate that is also native to the device, or
    // the current `SampleRate` if it is natively supported.
    // Assumes `InitContext` has already been called.

    if (!NativeSampleRates.contains(Type) || NativeSampleRates.at(Type).empty()) {
        throw std::runtime_error("No native sample rates found. Perhaps `InitContext` was not called before calling `GetConfigSampleRate`?");
    }

    const auto &native_sample_rates = NativeSampleRates.at(Type);
    if (u32(SampleRate) == 0) { // Default.
        // If `SampleRate` is 0, we want to choose the highest-priority sample rate that is native to the device.
        for (u32 sample_rate : PrioritizedSampleRates) {
            if (std::find(native_sample_rates.begin(), native_sample_rates.end(), sample_rate) != native_sample_rates.end()) return sample_rate;
        }
    } else {
        if (std::find(native_sample_rates.begin(), native_sample_rates.end(), SampleRate) != native_sample_rates.end()) return SampleRate;
    }

    // Either an explicit (non-default) sample rate that's not natively supported, or the device doesn't support any of the prioritized sample rates.
    // This will cause MA to perform automatic resampling.
    return SampleRate;
}

bool AudioDevice::IsStarted() const { return ma_device_is_started(Device.get()); }

using namespace ImGui;

void AudioDevice::Render() const {
    On.Draw();
    if (!IsStarted()) {
        TextUnformatted("Audio device is not started.");
        return;
    }

    SampleRate.Render(PrioritizedSampleRates);
    TextUnformatted(StringHelper::Capitalize(to_string(Type)).c_str());
    Name.Render(DeviceNames[Type]);
    // Format.Render(PrioritizedFormats); // See above - always using f32 format.

    if (TreeNode("Info")) {
        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];

        auto *device = Device.get();
        Text("[%s]", ma_get_backend_name(device->pContext->backend));

        if (Type == IO_In) {
            ma_device_get_name(device, ma_device_type_capture, name, sizeof(name), nullptr);
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
        } else {
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
        }
        TreePop();
    }
}

// todo implement for r8brain resampler
// todo I want to use this currently to support quality/fast resampling between _natively supported_ device sample rates.
//   Can I still use duplex mode in this case?
// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;
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
