#pragma once

#include <memory>
#include <optional>

#include "DeviceDataFormat.h"
#include "Project/Audio/AudioIO.h"

#include "miniaudio.h"

struct AudioDevice {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);

    struct UserData {
        AudioDevice *FlowGridDevice; // The FlowGrid device is added to the user data for every device.
        const void *User; // Arbitrary user data.
    };

    struct TargetConfig {
        std::optional<DeviceDataFormat> ClientFormat;
        std::optional<DeviceDataFormat> NativeFormat;
        std::string_view DeviceName;
    };

    struct Config {
        Config(IO, TargetConfig &&target);

        bool operator==(const Config &other) const {
            return ClientFormat == other.ClientFormat && NativeFormat == other.NativeFormat && DeviceName == other.DeviceName;
        }

        DeviceDataFormat ClientFormat;
        DeviceDataFormat NativeFormat;
        std::string DeviceName;
    };

    AudioDevice(IO, AudioCallback, TargetConfig &&target_config = {}, const void *client_user_data = nullptr);
    virtual ~AudioDevice();

    void SetConfig(TargetConfig &&config = {});

    static const std::vector<u32> PrioritizedSampleRates;
    static void ScanDevices();

    ma_device *Get() const { return Device.get(); }
    const ma_device_info *GetInfo() const { return &Info; }
    std::string GetName() const;
    bool IsDefault() const;

    bool IsInput() const { return Type == IO_In; }
    bool IsOutput() const { return Type == IO_Out; }

    bool IsStarted() const;
    bool IsNativeSampleRate(u32) const;

    const std::vector<DeviceDataFormat> &GetNativeFormats() const;
    const std::vector<const ma_device_info *> &GetAllInfos() const;

    ma_format GetNativeSampleFormat() const;
    u32 GetNativeChannels() const;
    u32 GetNativeSampleRate() const;
    DeviceDataFormat GetNativeFormat() const;

    u32 GetBufferFrames() const;

    const DeviceDataFormat &GetClientFormat() const { return _Config.ClientFormat; }

    void RenderInfo() const;

    void Stop();

    IO Type;
    AudioCallback Callback;
    UserData _UserData;

private:
    void Init();
    void Uninit();

    // The concrete computed config used to instantiate the device.
    // Should always mirror the MA device, with no default values except an emty name to indicate the devault device
    Config _Config;
    std::unique_ptr<ma_device> Device;

    ma_device_info Info;
};
