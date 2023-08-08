#include "AudioDevice.h"

#include <range/v3/range/conversion.hpp>

#include "Project/Audio/Graph/AudioGraphAction.h"
#include "UI/HelpMarker.h"

#include "miniaudio.h"

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

    static string GetDeviceName(const ma_device_info *info) { return !info || info->isDefault ? "" : info->name; }
    static string GetDeviceDisplayName(const ma_device_info *info) { return !info ? "None" : (string(info->name) + (info->isDefault ? "*" : "")); }

    const ma_device_info *GetDeviceInfo(IO type, string_view name) const {
        for (const auto *info : DeviceInfos[type]) {
            if ((name.empty() && info->isDefault) || info->name == name) return info;
        }
        return nullptr;
    }

    bool IsNativeSampleRate(IO type, u32 sample_rate) const {
        const auto &native_data_formats = NativeDataFormats[type];
        return std::any_of(native_data_formats.begin(), native_data_formats.end(), [sample_rate](const auto &df) { return df.SampleRate == sample_rate; });
    }

    u32 FindNearestNativeSampleRate(IO type, u32 target) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        // `min_element` requires a forward iterator, so we need to convert the range to a vector.
        const auto all_sample_rates = NativeDataFormats[type] | std::views::transform([](const auto &df) { return df.SampleRate; }) | ranges::to<std::vector<u32>>;
        // Find the nearest sample rate, favoring higher sample rates if there is a tie.
        return *std::min_element(all_sample_rates.begin(), all_sample_rates.end(), [target](u32 a, u32 b) {
            auto diff_a = std::abs(s64(a) - target);
            auto diff_b = std::abs(s64(b) - target);
            return diff_a < diff_b || (diff_a == diff_b && a > b);
        });
    }

    // If `sample_rate_target == 0`, returns the the highest-priority sample rate that is also native to the device,
    // If `sample_rate_target != 0`, returns the provided `sample_rate_target` if it is natively supported, or the nearest native sample rate otherwise.
    u32 GetHighestPriorityNativeSampleRate(IO type, u32 sample_rate_target) {
        if (NativeDataFormats[type].empty()) throw std::runtime_error(std::format("No native audio {} formats found.", to_string(type)));

        const auto &native_data_formats = NativeDataFormats[type];
        if (sample_rate_target == 0) { // Default.
            // By default, we want to choose the highest-priority sample rate that is native to the device.
            for (u32 sample_rate : AudioDevice::PrioritizedSampleRates) {
                if (IsNativeSampleRate(type, sample_rate)) return sample_rate;
            }
            // The device doesn't natively support any of the prioritized sample rates. Return the first native sample rate.
            return native_data_formats[0].SampleRate;
        } else {
            // Specific sample rate requested.
            if (IsNativeSampleRate(type, sample_rate_target)) return sample_rate_target;
        }

        // A specific (non-default) sample rate is configured that's not natively supported.
        // Return the nearest native sample rate.
        return FindNearestNativeSampleRate(type, sample_rate_target);
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
    std::vector<DeviceDataFormat> NativeDataFormats[IO_Count];
};

static std::unique_ptr<Context> AudioContext;
static u32 DeviceInstanceCount = 0; // Reference count for the audio context. When this goes from nonzero to zero, the context is destroyed.

AudioDevice::AudioDevice(ComponentArgs &&args, IO type, u32 client_sample_rate, AudioDevice::AudioCallback callback, void *client_user_data)
    : Component(std::move(args)), Type(type), Callback(callback), _UserData({this, client_user_data}) {
    const Field::References listened_fields{Name, Format};
    for (const Field &field : listened_fields) field.RegisterChangeListener(this);

    if (!AudioContext) AudioContext = std::make_unique<Context>();
    Init(client_sample_rate);
    DeviceInstanceCount++;
}

AudioDevice::~AudioDevice() {
    Uninit();
    Field::UnregisterChangeListener(this);

    DeviceInstanceCount--;
    if (DeviceInstanceCount == 0) AudioContext.reset();
}

void AudioDevice::OnFieldChanged() {
    if (Format.IsChanged()) {
        // If format was just toggled on and has never been set, set its values to the current device format.
        // This does not require a restart, since the format has not changed.
        if (Format && Format->SampleRate == 0u) UpdateFormat();
    }
    // todo when toggled off but the new followed format is the same as the previous user-specified format, we also don't need to restart.
    //   Implement something like a `GetFollowedFormat` that returns the format that will result from the restart, and use this in `Init` as well.
    if (Name.IsChanged() || (Format.IsChanged() && !Format) || (Format && Format->ToDeviceDataFormat() != GetNativeFormat())) {
        Uninit();
        Init(ClientSampleRate);
    }
}

void AudioDevice::Init(u32 client_sample_rate) {
    Device = std::make_unique<ma_device>();

    const ma_device_id *device_id = nullptr;
    for (const ma_device_info *info : AudioContext->DeviceInfos[Type]) {
        if (!info->isDefault && info->name == Name) {
            device_id = &(info->id);
            break;
        }
    }

    ma_device_config config = ma_device_config_init(IsInput() ? ma_device_type_capture : ma_device_type_playback);
    if (IsInput()) {
        config.capture.pDeviceID = device_id;
        config.capture.format = ma_format_f32;
        // config.capture.channels = Format.Channels;
        config.capture.channels = 1; // todo handle > 1 channels
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
        // config.playback.channels = Format.Channels;
        config.playback.channels = 1; // todo handle > 1 channels
    }

    config.dataCallback = Callback;
    config.pUserData = &_UserData;

    u32 target_sample_rate = Format ? Format->SampleRate : client_sample_rate; // Favor the user-selected format.
    u32 native_sample_rate = AudioContext->GetHighestPriorityNativeSampleRate(Type, target_sample_rate);
    ClientSampleRate = client_sample_rate == 0 ? native_sample_rate : client_sample_rate;

    u32 from_sample_rate = IsInput() ? native_sample_rate : ClientSampleRate;
    u32 to_sample_rate = IsInput() ? ClientSampleRate : native_sample_rate;

    config.sampleRate = ClientSampleRate;
    // Format/channels don't matter here.
    config.resampling = ma_resampler_config_init(ma_format_unknown, 0, from_sample_rate, to_sample_rate, ma_resample_algorithm_linear);
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer writes to every output frame.
    config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.

    ma_result result = ma_device_init(nullptr, &config, Device.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(Type), int(result)));

    // The device may have a different configuration than what we requested. Update the fields to reflect.
    Name.Set_(Context::GetDeviceName(AudioContext->GetDeviceInfo(Type, Name)));
    UpdateFormat();

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
    if (IsStarted()) ma_device_stop(Device.get());
    ma_device_uninit(Device.get());
    Device.reset();
}

string AudioDevice::GetName() const {
    const auto *info = AudioContext->GetDeviceInfo(Type, Name);
    return info ? info->name : "";
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

int AudioDevice::GetNativeSampleFormat() const {
    if (!Device) return ma_format_f32;
    if (IsInput()) return Device->capture.internalFormat;
    return Device->playback.internalFormat;
}

bool AudioDevice::IsNativeSampleRate(u32 sample_rate) const { return AudioContext->IsNativeSampleRate(Type, sample_rate); }

void AudioDevice::SetClientSampleRate(u32 client_sample_rate) {
    if (client_sample_rate == ClientSampleRate) return;

    // For now at least, just restart the device even if there is a resampler, since the device data converter is not intended
    // to be adjusted like this, and it leaves other internal fields out of sync.
    // I'm pretty sure we could just manually update the `internalSampleRate` in addition to this, but other things may be needed.
    // Also, it seems performant enough to just always restart the device.
    // if (IsInput()) {
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

string AudioDevice::DataFormat::GetFormatName(int format) { return DeviceDataFormat::GetFormatName(format); }

void AudioDevice::DataFormat::Set(DeviceDataFormat &&format) const {
    SampleFormat.Set(format.SampleFormat);
    Channels.Set(format.Channels);
    SampleRate.Set(format.SampleRate);
}

DeviceDataFormat AudioDevice::GetNativeFormat() const { return {GetNativeSampleFormat(), GetNativeChannels(), GetNativeSampleRate()}; }

void AudioDevice::UpdateFormat() {
    if (!Format) return;

    const auto native_format = GetNativeFormat();
    Format->SampleFormat.Set_(native_format.SampleFormat);
    Format->Channels.Set_(native_format.Channels);
    Format->SampleRate.Set_(native_format.SampleRate);
}

void AudioDevice::DataFormat::Render() const {
    const auto *device = static_cast<const AudioDevice *>(Parent->Parent);
    if (BeginCombo(ImGuiLabel.c_str(), DeviceDataFormat{SampleFormat, Channels, SampleRate}.ToString().c_str())) {
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

    SameLine(); // Assumes 'Delete' button is rendered by the graph immediately before this.
    if (Button("Rescan")) AudioContext->ScanDevices();

    SetNextItemWidth(GetFontSize() * 14);
    const auto *device_info = AudioContext->GetDeviceInfo(Type, Name);
    if (BeginCombo(Name.ImGuiLabel.c_str(), Context::GetDeviceDisplayName(device_info).c_str())) {
        for (const auto *other_device_info : AudioContext->DeviceInfos[Type]) {
            const bool is_selected = device_info == other_device_info;
            if (Selectable(Context::GetDeviceDisplayName(other_device_info).c_str(), is_selected)) Name.IssueSet(Context::GetDeviceName(other_device_info));
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    Name.HelpMarker();

    SetNextItemWidth(GetFontSize() * 14);
    bool follow_graph_format = !Format;
    if (Checkbox("Follow graph format", &follow_graph_format)) Format.IssueToggle();
    SameLine();
    fg::HelpMarker(std::format(
        "When checked, this {0} device automatically follows the owning graph's sample rate and format. "
        "When the graph's sample rate changes, the device will be updated to use the native sample rate nearest to the graph's.\n\n"
        "When unchecked, this {0} device will be pinned to the selected native format, and will convert from the {1} format to the {2} format.\n"
        "See 'Device info' section for details on the device's current format conversion configuration.",
        to_string(Type), IsInput() ? "device" : "graph", IsInput() ? "graph" : "device"
    ));

    if (Format) {
        Format->Draw();
    } else {
        TextUnformatted(GetNativeFormat().ToString().c_str());
    }

    if (ImGui::TreeNode("Device info")) {
        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        auto *device = Device.get();
        ma_device_get_name(device, IsInput() ? ma_device_type_capture : ma_device_type_playback, name, sizeof(name), nullptr);
        Text("%s (%s)", name, IsInput() ? "Capture" : "Playback");
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
        TreePop();
    }
}
