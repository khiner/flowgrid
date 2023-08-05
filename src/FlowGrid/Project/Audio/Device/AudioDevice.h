#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"
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
    bool IsStarted() const;

    bool IsNativeSampleRate(u32) const;
    void SetClientSampleRate(u32); // The graph sample rate.

    // Mirrors `DeviceDataFormat`.
    // A `SampleRate` of 0 indicates we have not initialized yet, and we choose the default device data format.
    struct DataFormat : Component {
        using Component::Component;

        static std::string GetFormatName(int);

        Prop(Enum, SampleFormat, GetFormatName); // If set to ma_format_unknown, all sample formats are supported. */
        Prop(UInt, Channels);
        Prop(UInt, SampleRate);

    private:
        void Render() const override; // Rendered as a dropdown.
    };

    Prop(String, Name); // todo only use name for explicit user device selection.
    Prop_(
        DataFormat, Format,
        "?The native device data format .\n"
        "All data formats natively supported by the audio device are allowed.\n"
        "If this format is different from that of the audio graph, the audio will be converted to/from this native format."
    );

private:
    void Render() const override;

    void Init(u32 client_sample_rate);
    void Uninit();

    u32 ClientSampleRate{0};
    IO Type;
    AudioCallback Callback;

    UserData _UserData;

    std::unique_ptr<ma_device> Device;
};
