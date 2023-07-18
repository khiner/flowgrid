#pragma once

#include "AudioAction.h"
#include "AudioInputDevice.h"
#include "AudioOutputDevice.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct ma_device;

struct Audio : Component, Actionable<Action::Audio::Any> {
    Audio(ComponentArgs &&);
    ~Audio();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    // These callbacks delegate to `Graph.Audio(Input|Output)Callback`.
    // We use this indirection so we can initialize `AudioDevice`s with a callback before initializing `AudioGraph`
    // (which depends on the `AudioDevice`s being initialized).
    static void AudioInputCallback(ma_device *, void *output, const void *input, u32 frame_count);
    static void AudioOutputCallback(ma_device *, void *output, const void *input, u32 frame_count);

    Prop(AudioInputDevice, InputDevice, AudioInputCallback);
    Prop(AudioOutputDevice, OutputDevice, AudioOutputCallback);
    Prop(AudioGraph, Graph, InputDevice, OutputDevice);
    Prop(Faust, Faust);

protected:
    void Render() const override;
};
