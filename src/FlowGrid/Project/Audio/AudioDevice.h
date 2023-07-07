#pragma once

#include "AudioIO.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"

struct ma_device;
// Corresponds to `ma_device`.
struct AudioDevice : Component, Field::ChangeListener {
    using AudioCallback = void (*)(ma_device *, void *, const void *, Count);

    AudioDevice(ComponentArgs &&, AudioCallback);
    ~AudioDevice();

    static const std::vector<U32> PrioritizedSampleRates;
    static const string GetFormatName(int); // `ma_format` argmument is converted to an `int`.
    static const string GetSampleRateName(U32);

    void OnFieldChanged() override;

    void Start() const;
    void Stop() const;
    bool IsStarted() const;

    Prop_(Bool, On, "?When the audio device is turned off, the audio graph is destroyed and no audio processing takes place.", true);
    Prop(String, InDeviceName);
    Prop(String, OutDeviceName);
    Prop_(Enum, InFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
    Prop_(Enum, OutFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
    Prop(UInt, InChannels, 1);
    Prop(UInt, OutChannels, 1);
    Prop_(UInt, SampleRate, "?An asterisk (*) indicates the sample rate is natively supported by the audio device. All non-native sample rates require resampling.", GetSampleRateName);

protected:
    void Render() const override;

private:
    void Init();
    void Uninit();

    AudioCallback Callback;
};

extern const AudioDevice &audio_device;
