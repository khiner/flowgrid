#include "AudioDevice.h"

#include "imgui.h"
#include "miniaudio.h"

#include "Helper/String.h"

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

static ma_context AudioContext;
static u16 AudioContextInitializedCount = 0;

static ma_device_info DeviceInfo;

AudioDevice::AudioDevice(ComponentArgs &&args, AudioDevice::AudioCallback callback)
    : Component(std::move(args)), Callback(callback) {
    const Field::References listened_fields{On, Name, Format, Channels, SampleRate};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
}

AudioDevice::~AudioDevice() {
    Field::UnregisterChangeListener(this);
}

void AudioDevice::OnFieldChanged() {
    if (On.IsChanged() ||
        Name.IsChanged() ||
        Format.IsChanged() ||
        Channels.IsChanged() ||
        SampleRate.IsChanged()) {
        const bool is_started = IsStarted();
        if (On && !is_started) {
            Init();
        } else if (!On && is_started) {
            Uninit();
        } else if (is_started) {
            // todo no need to completely reset in some cases (like when only format has changed).
            // todo sample rate conversion is happening even when choosing a SR that is native to both input & output, if it's not the highest-priority SR.
            Uninit();
            Init();
        }
    }
}

ma_device_type AudioDevice::GetMaDeviceType() const {
    return GetIoType() == IO_In ? ma_device_type_capture : ma_device_type_playback;
}

string AudioDevice::GetFormatName(int format) const {
    const bool is_native = std::find(NativeFormats.begin(), NativeFormats.end(), format) != NativeFormats.end();
    return ::std::format("{}{}", ma_get_format_name((ma_format)format), is_native ? "*" : "");
}
string AudioDevice::GetSampleRateName(u32 sample_rate) const {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return std::format("{}{}", to_string(sample_rate), is_native ? "*" : "");
}

static std::vector<ma_device_info *> DeviceInfos[IO_Count];
static std::vector<string> DeviceNames[IO_Count];

const ma_device_id *AudioDevice::GetDeviceId(string_view device_name) const {
    for (const ma_device_info *info : DeviceInfos[GetIoType()]) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

void AudioDevice::InitContext() {
    AudioContextInitializedCount++;
    if (AudioContextInitializedCount > 1) return;

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

    result = ma_context_get_device_info(&AudioContext, GetMaDeviceType(), nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio {} device info: {}", to_string(GetIoType()), result));

    // todo need to verify that the cross-product of these formats & sample rates are supported natively.
    // Create a new format type that mirrors MA's (with sample format and sample rate).
    for (u32 i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
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

void AudioDevice::UninitContext() {
    AudioContextInitializedCount--;
    if (AudioContextInitializedCount > 0) return;

    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error shutting down audio context: {}", result));
}

bool AudioDevice::IsStarted() const { return ma_device_is_started(Get()); }

using namespace ImGui;

void AudioDevice::Render() const {
    const IO io = GetIoType();

    On.Draw();
    if (!IsStarted()) {
        TextUnformatted("Audio device is not started.");
        return;
    }
    SampleRate.Render(PrioritizedSampleRates);
    TextUnformatted(StringHelper::Capitalize(to_string(io)).c_str());
    Name.Render(DeviceNames[io]);
    // Format.Render(PrioritizedFormats); // See above - always using f32 format.

    if (TreeNode("Info")) {
        auto *device = Get();
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
