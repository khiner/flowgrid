#include "AudioOutputDevice.h"

#include "miniaudio.h"

#include "Helper/String.h"

static ma_device MaDevice;

AudioOutputDevice::AudioOutputDevice(ComponentArgs &&args, AudioDevice::AudioCallback callback)
    : AudioDevice(std::move(args), std::move(callback)) {
    Init();
}

AudioOutputDevice::~AudioOutputDevice() {
    Uninit();
}

ma_device *AudioOutputDevice::Get() const { return &MaDevice; }

void AudioOutputDevice::Init() {
    AudioDevice::InitContext();

    ma_device_config config;
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = GetDeviceId(Name);
    config.playback.format = ma_format_f32;
    config.playback.channels = Channels;
    config.dataCallback = Callback;
    config.sampleRate = GetConfigSampleRate();
    // config.pUserData = pMyCustomData; // Can be accessed from the device object (device.pUserData).
    config.noPreSilencedOutputBuffer = true; // The audio graph already ensures the output buffer already writes to every output frame.

    int result = ma_device_init(nullptr, &config, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio {} device: {}", to_string(GetIoType()), result));

    // The device may have a different configuration than what we requested.
    // Update the fields to reflect the actual device configuration.
    if (MaDevice.playback.name != Name) Name.Set_(MaDevice.playback.name);
    if (MaDevice.playback.format != Format) Format.Set_(MaDevice.playback.format);
    if (MaDevice.playback.channels != Channels) Channels.Set_(MaDevice.playback.channels);
    if (MaDevice.sampleRate != SampleRate) SampleRate.Set_(MaDevice.sampleRate);

    result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio {} device: {}", to_string(GetIoType()), result));
}

void AudioOutputDevice::Uninit() {
    if (IsStarted()) {
        const int result = ma_device_stop(&MaDevice);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error stopping audio {} device: {}", to_string(GetIoType()), result));
    }
    ma_device_uninit(&MaDevice);

    AudioDevice::UninitContext();
}
