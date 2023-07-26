#pragma once

#include "AudioIO.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"

#include "miniaudio.h"

struct ma_device;
// typedef struct ma_device_id ma_device_id;

// Corresponds to `ma_device`.
struct AudioDevice : Component, Field::ChangeListener {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);
    using UserData = void *;

    AudioDevice(ComponentArgs &&, IO, AudioCallback, UserData user_data = nullptr);
    virtual ~AudioDevice();

    void OnFieldChanged() override;

    bool IsStarted() const;
    std::string GetFormatName(int) const;
    std::string GetSampleRateName(u32) const;
    u64 GetBufferSize() const;

    Prop(Bool, On, true);
    Prop(String, Name);
    Prop(UInt, Channels, 1);
    Prop_(Enum, Format, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", [this](int f) { return GetFormatName(f); });
    // We initialize with a `SampleRate` of 0, which will choose the default device sample rate.
    Prop_(UInt, SampleRate, "?An asterisk (*) indicates the sample rate is natively supported by the audio device. All non-native sample rates require resampling.", [this](u32 sr) { return GetSampleRateName(sr); });

private:
    void Render() const override;

    const ma_device_id *GetDeviceId(string_view device_name) const;

    // Uses the current `SampleRate`, the `PrioritizedSampleRates` list, and the device's native sample rates
    // to determine the best sample rate with which to initialize the `ma_device`.
    u32 GetConfigSampleRate() const;

    bool IsNativeSampleRate(u32 sample_rate) const;
    bool IsNativeFormat(ma_format format) const;

    IO Type;
    AudioCallback Callback;
    UserData _UserData;

    std::unique_ptr<ma_device> Device;
};
