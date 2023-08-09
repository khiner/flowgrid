#pragma once

#include "DeviceDataFormat.h"
#include "Project/Audio/AudioIO.h"

#include "miniaudio.h"

struct AudioDevice {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);

    struct UserData {
        AudioDevice *FlowGridDevice; // The FlowGrid device is added to the user data for every device.
        void *User; // Arbitrary user data.
    };

    AudioDevice(IO, AudioCallback, std::optional<DeviceDataFormat> client_format, std::optional<DeviceDataFormat> native_format_target = {}, std::string_view device_name_target = "", void *client_user_data = nullptr);
    virtual ~AudioDevice();

    static const std::vector<u32> PrioritizedSampleRates;
    static void ScanDevices();

    ma_device *Get() const { return Device.get(); }
    const ma_device_info *GetInfo() const { return &Info; }
    std::string GetName() const;
    bool IsDefault() const;

    inline bool IsInput() const { return Type == IO_In; }
    inline bool IsOutput() const { return Type == IO_Out; }

    bool IsStarted() const;
    bool IsNativeSampleRate(u32) const;

    const std::vector<DeviceDataFormat> &GetNativeFormats() const;
    const std::vector<const ma_device_info *> &GetAllInfos() const;

    u32 GetNativeChannels() const;
    int GetNativeSampleFormat() const; // Convert to `ma_format`.
    u32 GetNativeSampleRate() const;
    const DeviceDataFormat &GetClientFormat() const { return ClientFormat; }
    DeviceDataFormat GetNativeFormat() const;

    void RenderInfo() const;

    void Init(std::optional<DeviceDataFormat> client_format, std::optional<DeviceDataFormat> native_format_target = {}, std::string_view device_name_target = "");
    void Uninit();

    IO Type;
    AudioCallback Callback;
    UserData _UserData;

private:
    DeviceDataFormat ClientFormat{}; // The concrete client format used to instantiate the device. No default values (e.g. `SampleRate != 0`).
    std::unique_ptr<ma_device> Device;

    ma_device_info Info;
};
