#pragma once

#include "AudioIO.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"

struct ma_device;

struct AudioDevice : Component, Field::ChangeListener {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);
    using UserData = void *;

    static const std::vector<u32> PrioritizedSampleRates;

    AudioDevice(ComponentArgs &&, IO, u32 client_sample_rate, AudioCallback, UserData user_data = nullptr);
    virtual ~AudioDevice();

    void OnFieldChanged() override;

    ma_device *Get() const { return Device.get(); }

    bool IsStarted() const;
    std::string GetFormatName(int) const;
    std::string GetSampleRateName(u32) const;

    bool IsNativeSampleRate(u32) const;
    void SetClientSampleRate(u32); // The graph sample rate.

    Prop(String, Name);
    Prop(UInt, Channels, 1);
    Prop_(Enum, Format, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", [this](int f) { return GetFormatName(f); });

    // We initialize with a sample rate of 0, which will choose the default device sample rate.
    Prop_(
        UInt, NativeSampleRate,
        "?The native device processing sample rate.\n"
        "All sample rates natively supported by the audio device are allowed.\n"
        "If this sample rate is different from that of the audio graph, the audio will be converted from this native sample rate.",
        [this](u32 sr) { return GetSampleRateName(sr); }
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
