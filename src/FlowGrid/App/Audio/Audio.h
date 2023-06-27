#pragma once

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct ma_device;

struct Audio : Component, Actionable<Action::Audio::Any>, Field::ChangeListener {
    Audio(ComponentArgs &&);
    ~Audio();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void OnFieldChanged() override;

    // Just delegates to `AudioGraph::AudioCallback`.
    // We use this indirection so we can initialize `AudioDevice` with a callback before initializing `AudioGraph`
    // (which depends on `AudioDevice` being initialized).
    static void AudioCallback(ma_device *, void *output, const void *input, Count frame_count);

    Prop(AudioDevice, Device, AudioCallback);
    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;
};
