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
// Abstract class. Sett `AudioInputDevice`/`AudioOutputDevice` for concrete implementations.
struct AudioDevice : Component, Field::ChangeListener {
    using AudioCallback = void (*)(ma_device *, void *, const void *, u32);

    AudioDevice(ComponentArgs &&, AudioCallback);
    virtual ~AudioDevice();

    std::string GetFormatName(int) const; // `ma_format` argmument is converted to an `int`.
    std::string GetSampleRateName(u32) const;

    void OnFieldChanged() override;

    virtual ma_device *Get() const = 0;
    virtual IO GetIoType() const = 0;
    ma_device_type GetMaDeviceType() const;

    bool IsStarted() const;

    Prop(Bool, On, true);
    Prop(String, Name);
    Prop(UInt, Channels, 1);
    Prop_(Enum, Format, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", [this](int f) { return GetFormatName(f); });
    // We initialize with a `SampleRate` of 0, which will choose the default device sample rate.
    Prop_(UInt, SampleRate, "?An asterisk (*) indicates the sample rate is natively supported by the audio device. All non-native sample rates require resampling.", [this](u32 sr) { return GetSampleRateName(sr); });

protected:
    void Render() const override;

    void InitContext();
    void UninitContext();

    const ma_device_id *GetDeviceId(string_view device_name) const;

    virtual void Init() = 0;
    virtual void Uninit() = 0;

    AudioCallback Callback;

    static const std::vector<u32> PrioritizedSampleRates;
    std::vector<ma_format> NativeFormats;
    std::vector<u32> NativeSampleRates;
};
