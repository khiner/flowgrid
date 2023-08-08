#pragma once

#include "Core/Container/Optional.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"

#include "DeviceDataFormat.h"
#include "Project/Audio/AudioIO.h"

struct ma_device;

struct AudioDevice : Component, Field::ChangeListener {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);

    struct UserData {
        AudioDevice *FlowGridDevice; // The FlowGrid device is added to the user data for every device.
        void *User; // Arbitrary user data.
    };

    static const std::vector<u32> PrioritizedSampleRates;

    AudioDevice(ComponentArgs &&, IO, u32 client_sample_rate, AudioCallback, void *client_user_data = nullptr);
    virtual ~AudioDevice();

    void OnFieldChanged() override;

    ma_device *Get() const { return Device.get(); }

    std::string GetName() const; // `Name` can be empty if the device is the default device. This returns the actual device name.

    inline bool IsInput() const { return Type == IO_In; }
    inline bool IsOutput() const { return Type == IO_Out; }

    bool IsStarted() const;
    bool IsNativeSampleRate(u32) const;

    u32 GetNativeChannels() const;
    int GetNativeSampleFormat() const; // Convert to `ma_format`.

    u32 GetNativeSampleRate() const;
    u32 GetClientSampleRate() const { return ClientSampleRate; }
    void SetClientSampleRate(u32); // The graph sample rate.

    // Mirrors `DeviceDataFormat`, as a component.
    // The device `Format` is its _native_ format.
    struct DataFormat : Component {
        using Component::Component;

        static std::string GetFormatName(int);

        void Set(DeviceDataFormat &&) const;
        operator DeviceDataFormat() const { return {SampleFormat, Channels, SampleRate}; }

        Prop(Enum, SampleFormat, GetFormatName);
        Prop(UInt, Channels);
        Prop(UInt, SampleRate);

    private:
        void Render() const override; // Rendered as a dropdown.
    };

    // When this is either empty or a device name that does not exist, the default device is used.
    Prop_(String, Name, "?An asterisk (*) indicates the default device.");

    // The format is considered "default" if it is not set.
    // When a format is set to the default, it follows the owning graph's sample rate.
    // When the graph's sample rate changes, each device node is updated to select the native format with the sample rate nearest to the graph's.
    Prop(Optional<DataFormat>, Format);

private:
    void Render() const override;

    void Init(u32 client_sample_rate);
    void Uninit();

    // If this device's `Format` is set (if it has been explicitly chosen by the user), update its fields to reflect the current native device config.
    void UpdateFormat();
    DeviceDataFormat GetNativeFormat() const;

    u32 ClientSampleRate{0};
    IO Type;
    AudioCallback Callback;
    UserData _UserData;
    std::unique_ptr<ma_device> Device;
};
