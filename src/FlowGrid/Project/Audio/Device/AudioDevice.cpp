#include "AudioDevice.h"

#include "DeviceDataFormat.h"
#include "Project/Audio/Graph/AudioGraphAction.h"

#include "imgui.h"

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

    bool IsNativeSampleRate(IO type, u32 sample_rate) const {
        const auto &native_data_formats = NativeDataFormats[type];
        return std::any_of(native_data_formats.begin(), native_data_formats.end(), [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
    }

    // The native sample rate list may have a different prioritized order than our priority list.
    // If `sample_rate_target == 0`, returns the the highest-priority sample rate that is also native to the device,
    // If `sample_rate_target != 0`, returns the provided `sample_rate_target` if it is natively supported, or the first native sample rate otherwise.
    // Assumes `NativeSampleRates` has already been populated.
    u32 GetHighestPriorityNativeSampleRate(IO type, u32 sample_rate_target) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        const auto &native_data_formats = NativeDataFormats[type];
        if (sample_rate_target == 0) { // Default.
            // By default, we want to choose the highest-priority sample rate that is native to the device.
            for (u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
                if (IsNativeSampleRate(type, sample_rate)) return sample_rate;
            }
        } else {
            // Specific sample rate requested.
            if (IsNativeSampleRate(type, sample_rate_target)) return sample_rate_target;
        }

        // Either a specific (non-default) sample rate is configured that's not natively supported,
        // or the device doesn't natively support any of the prioritized sample rates.
        // We return the first native sample rate.
        return native_data_formats[0].SampleRate;
    }

    void ScanDevices() {
        static u32 PlaybackDeviceCount, CaptureDeviceCount;
        static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
        ma_result result = ma_context_get_devices(&MaContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", int(result)));

        for (IO io : IO_All) DeviceInfos[io].clear();
        for (IO io : IO_All) DeviceNames[io].clear();
        for (u32 i = 0; i < CaptureDeviceCount; i++) {
            DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
            DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
        }
        for (u32 i = 0; i < PlaybackDeviceCount; i++) {
            DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
            DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
        }

        for (const IO io : IO_All) {
            NativeDataFormats[io].clear();

            ma_device_info DeviceInfo;
            result = ma_context_get_device_info(&MaContext, io == IO_In ? ma_device_type_capture : ma_device_type_playback, nullptr, &DeviceInfo);
            if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio {} device info: {}", to_string(io), int(result)));

            for (u32 i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
                const auto &df = DeviceInfo.nativeDataFormats[i];
                NativeDataFormats[io].emplace_back(df.format, df.channels, df.sampleRate);
            }
        }
    }

    ma_context MaContext;
    std::vector<ma_device_info *> DeviceInfos[IO_Count];
    std::vector<string> DeviceNames[IO_Count];
    std::vector<DeviceDataFormat> NativeDataFormats[IO_Count];
};

static std::unique_ptr<Context> AudioContext;

static u32 DeviceInstanceCount = 0; // Reference count for the audio context. When this goes from nonzero to zero, the context is destroyed.

AudioDevice::AudioDevice(ComponentArgs &&args, IO type, u32 client_sample_rate, AudioDevice::AudioCallback callback, void *client_user_data)
    : Component(std::move(args)), Type(type), Callback(callback), _UserData({this, client_user_data}) {
    const Field::References listened_fields{Name, Format.SampleFormat, Format.Channels, Format.SampleRate};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);
    Init(client_sample_rate);
}

AudioDevice::~AudioDevice() {
    Uninit();
    Field::UnregisterChangeListener(this);
}

void AudioDevice::Init(u32 client_sample_rate) {
    if (!AudioContext) AudioContext = std::make_unique<Context>();

    Device = std::make_unique<ma_device>();

    const ma_device_id *device_id = nullptr;
    // todo only use name for explicit user device selection.
    for (const ma_device_info *info : AudioContext->DeviceInfos[Type]) {
        if (info->name == Name) {
            device_id = &(info->id);
            break;
        }
    }

    ma_device_config config = ma_device_config_init(Type == IO_In ? ma_device_type_capture : ma_device_type_playback);
    if (Type == IO_In) {
        config.capture.pDeviceID = device_id;
        config.capture.format = ma_format_f32;
        // config.capture.channels = Format.Channels;
        config.capture.channels = 1; // todo handle > 1 channels
        // `noFixedSizedCallback` is more efficient, and seems to be ok.
        // Also seems fine for the output device, but only using it for the input device for now.
        config.noFixedSizedCallback = true;
    } else {
        config.playback.pDeviceID = device_id;
        config.playback.format = ma_format_f32;
        // config.playback.channels = Format.Channels;
        config.playback.channels = 1; // todo handle > 1 channels
    }

    config.dataCallback = Callback;
    config.pUserData = &_UserData;

    u32 native_sample_rate_valid = AudioContext->GetHighestPriorityNativeSampleRate(Type, Format.SampleRate);
    ClientSampleRate = client_sample_rate == 0 ? native_sample_rate_valid : client_sample_rate;

    u32 from_sample_rate = Type == IO_In ? native_sample_rate_valid : ClientSampleRate;
    u32 to_sample_rate = Type == IO_In ? ClientSampleRate : native_sample_rate_valid;

    config.sampleRate = ClientSampleRate;
    // Format/channels/rate doesn't matter here.
    config.resampling = ma_resampler_config_init(ma_format_unknown, 0, from_sample_rate, to_sample_rate, ma_resample_algorithm_linear);
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer already writes to every output frame.
    config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.

    ma_result result = ma_device_init(nullptr, &config, Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(Type), int(result)));

    // The device may have a different configuration than what we requested.
    // Update the fields to reflect the actual device configuration.
    if (Type == IO_Out) {
        if (Device->playback.name != Name) Name.Set_(Device->playback.name);
        if (Device->playback.format != Format.SampleFormat) Format.SampleFormat.Set_(Device->playback.format);
        if (Device->playback.channels != Format.Channels) Format.Channels.Set_(Device->playback.channels);
        if (Device->playback.internalSampleRate != Format.SampleRate) Format.SampleRate.Set_(Device->playback.internalSampleRate);
    } else {
        if (Device->capture.name != Name) Name.Set_(Device->capture.name);
        if (Device->capture.format != Format.SampleFormat) Format.SampleFormat.Set_(Device->capture.format);
        if (Device->capture.channels != Format.Channels) Format.Channels.Set_(Device->capture.channels);
        if (Device->capture.internalSampleRate != Format.SampleRate) Format.SampleRate.Set_(Device->capture.internalSampleRate);
    }

    Device->onNotification = [](const ma_device_notification *notification) {
        switch (notification->type) {
            case ma_device_notification_type_started:
                break;
            case ma_device_notification_type_stopped:
                break;
            case ma_device_notification_type_rerouted:
                // Can happen e.g. the default device is changed.
                AudioContext->ScanDevices();
                {
                    const auto *user_data = reinterpret_cast<const UserData *>(notification->pDevice->pUserData);
                    const auto *self = user_data->FlowGridDevice;
                    self->Name.IssueSet(self->Type == IO_Out ? notification->pDevice->playback.name : notification->pDevice->capture.name);
                }
                break;
            case ma_device_notification_type_interruption_began:
                break;
            case ma_device_notification_type_interruption_ended:
                break;
        }
    };

    result = ma_device_start(Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio {} device: {}", to_string(Type), int(result)));

    DeviceInstanceCount++;

    // todo option to change dither mode, only present when used
    // config.capture.format = ToAudioFormat(InFormat);
    // config.playback.format = ToAudioFormat(OutFormat);

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
    if (DeviceInstanceCount == 0) throw std::runtime_error("Audio device instance count is already zero.");

    DeviceInstanceCount--;

    if (IsStarted()) ma_device_stop(Device.get());
    ma_device_uninit(Device.get());
    Device.reset();

    if (DeviceInstanceCount == 0) AudioContext.reset();
}

void AudioDevice::OnFieldChanged() {
    if (Name.IsChanged() || Format.IsChanged()) {
        Uninit();
        Init(ClientSampleRate);
    }
}

bool AudioDevice::IsNativeSampleRate(u32 sample_rate) const {
    return AudioContext->IsNativeSampleRate(Type, sample_rate);
}

void AudioDevice::SetClientSampleRate(u32 client_sample_rate) {
    if (client_sample_rate == ClientSampleRate) return;

    // For now at least, just restart the device even if there is a resampler, since the device data converter is not intended
    // to be adjusted like this, and it leaves other internal fields out of sync.
    // I'm pretty sure we could just manually update the `internalSampleRate` in addition to this, but other things may be needed.
    // Also, it seems performant enough to just always restart the device.
    // if (Type == IO_In) {
    //     if (Device->capture.converter.hasResampler) {
    //         ma_data_converter_set_rate(&Device->capture.converter, Device->capture.converter.resampler.sampleRateIn, sample_rate);
    //     }
    // } else {
    //     if (Device->playback.converter.hasResampler) {
    //         ma_data_converter_set_rate(&Device->playback.converter, sample_rate, Device->playback.converter.resampler.sampleRateOut);
    //     }
    // }

    Uninit();
    Init(client_sample_rate);
}

bool AudioDevice::IsStarted() const { return ma_device_is_started(Device.get()); }

using namespace ImGui;

// ma_format ToAudioFormat(const Audio::IoFormat format) {
//     switch (format) {
//         case Audio::IoFormat_Native: return ma_format_unknown;
//         case Audio::IoFormat_F32: return ma_format_f32;
//         case Audio::IoFormat_S32: return ma_format_s32;
//         case Audio::IoFormat_S16: return ma_format_s16;
//         case Audio::IoFormat_S24: return ma_format_s24;
//         case Audio::IoFormat_U8: return ma_format_u8;
//         default: return ma_format_unknown;
//     }
// }
// Audio::IoFormat ToAudioFormat(const ma_format format) {
//     switch (format) {
//         case ma_format_unknown: return Audio::IoFormat_Native;
//         case ma_format_f32: return Audio::IoFormat_F32;
//         case ma_format_s32: return Audio::IoFormat_S32;
//         case ma_format_s16: return Audio::IoFormat_S16;
//         case ma_format_s24: return Audio::IoFormat_S24;
//         case ma_format_u8: return Audio::IoFormat_U8;
//         default: return Audio::IoFormat_Native;
//     }
// }

string AudioDevice::DataFormat::GetFormatName(int format) { return DeviceDataFormat::GetFormatName(ma_format(format)); }

void AudioDevice::DataFormat::Render() const {
    const auto *device = static_cast<const AudioDevice *>(Parent);
    if (BeginCombo(ImGuiLabel.c_str(), DeviceDataFormat{ma_format(int(SampleFormat)), Channels, SampleRate}.ToString().c_str())) {
        for (const auto &df : AudioContext->NativeDataFormats[device->Type]) {
            const bool is_selected = SampleFormat == df.SampleFormat && Channels == df.Channels && SampleRate == df.SampleRate;
            if (Selectable(df.ToString().c_str(), is_selected)) Action::AudioGraph::SetDeviceDataFormat{Id, df.SampleFormat, df.Channels, df.SampleRate}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void AudioDevice::Render() const {
    if (!AudioContext || !IsStarted()) {
        TextUnformatted("Device is not started.");
        return;
    }

    SetNextItemWidth(GetFontSize() * 14);
    Name.Render(AudioContext->DeviceNames[Type]);
    SetNextItemWidth(GetFontSize() * 14);
    Format.Draw();

    if (TreeNode("Device info")) {
        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        auto *device = Device.get();
        ma_device_get_name(device, Type == IO_In ? ma_device_type_capture : ma_device_type_playback, name, sizeof(name), nullptr);
        Text("%s (%s)", name, Type == IO_In ? "Capture" : "Playback");
        Text("Backend: %s", ma_get_backend_name(device->pContext->backend));
        if (Type == IO_In) {
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
        TreePop();
    }
}
