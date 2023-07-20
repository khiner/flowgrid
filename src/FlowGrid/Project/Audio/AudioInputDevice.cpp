#include "AudioInputDevice.h"

#include "miniaudio.h"

#include "Helper/String.h"

static ma_device MaDevice;

AudioInputDevice::AudioInputDevice(Component *parent, string_view path_segment, AudioDevice::AudioCallback callback, UserData user_data)
    : AudioDevice(parent, path_segment, std::move(callback), user_data) {
    Init();
}

AudioInputDevice::~AudioInputDevice() {
    Uninit();
}

ma_device *AudioInputDevice::Get() const { return &MaDevice; }

void AudioInputDevice::Init() {
    AudioDevice::InitContext();

    ma_device_config config;
    config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = GetDeviceId(Name);
    config.capture.format = ma_format_f32;
    config.capture.channels = Channels;
    config.sampleRate = GetConfigSampleRate();
    config.dataCallback = Callback;
    config.pUserData = _UserData;
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer already writes to every output frame.
    config.coreaudio.allowNominalSampleRateChange = true; // On Mac, allow changing the native system sample rate.

    int result = ma_device_init(nullptr, &config, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(GetIoType()), result));

    // The device may have a different configuration than what we requested.
    // Update the fields to reflect the actual device configuration.
    if (MaDevice.capture.name != Name) Name.Set_(MaDevice.capture.name);
    if (MaDevice.capture.format != Format) Format.Set_(MaDevice.capture.format);
    if (MaDevice.capture.channels != Channels) Channels.Set_(MaDevice.capture.channels);
    if (MaDevice.sampleRate != SampleRate) SampleRate.Set_(MaDevice.sampleRate);

    result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio {} device: {}", to_string(GetIoType()), result));
}

void AudioInputDevice::Uninit() {
    if (IsStarted()) {
        const int result = ma_device_stop(&MaDevice);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error stopping audio {} device: {}", to_string(GetIoType()), result));
    }
    ma_device_uninit(&MaDevice);

    AudioDevice::UninitContext();
}
