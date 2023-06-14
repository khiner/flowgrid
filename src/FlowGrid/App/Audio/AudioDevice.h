#pragma once

#include "AudioIO.h"
#include "Core/Field/Bool.h"
#include "Core/Field/Enum.h"
#include "Core/Field/Float.h"
#include "Core/Field/String.h"
#include "Core/Field/UInt.h"

struct ma_device;
// Corresponds to `ma_device`.
struct AudioDevice : Component, Drawable {
    using Component::Component;

    static const std::vector<U32> PrioritizedSampleRates;
    static const string GetFormatName(int); // `ma_format` argmument is converted to an `int`.
    static const string GetSampleRateName(U32);

    using Callback = void (*)(ma_device *, void *, const void *, Count);

    bool NeedsRestart() const;

    void Init(Callback callback);
    void Update(); // Update device based on current settings.
    void Uninit();

    void Start() const;
    void Stop() const;
    bool IsStarted() const;

    Prop_(Bool, On, "?When the audio device is turned off, the audio graph is destroyed and no audio processing takes place.", true);
    Prop_(Bool, Muted, "?Completely mute audio output device. All audio computation will still be performed, so this setting does not affect CPU load.", true);
    Prop(Float, Volume, 1.0); // Master volume. Corresponds to `ma_device_set_master_volume`.
    Prop(String, InDeviceName);
    Prop(String, OutDeviceName);
    Prop_(Enum, InFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
    Prop_(Enum, OutFormat, "?An asterisk (*) indicates the format is natively supported by the audio device. All non-native formats require conversion.", GetFormatName);
    Prop(UInt, InChannels, 1);
    Prop(UInt, OutChannels, 1);
    Prop_(UInt, SampleRate, "?An asterisk (*) indicates the sample rate is natively supported by the audio device. All non-native sample rates require resampling.", GetSampleRateName);

protected:
    void Render() const override;
};

extern const AudioDevice &audio_device;
