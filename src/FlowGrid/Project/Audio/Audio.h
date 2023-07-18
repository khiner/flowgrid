#pragma once

#include "AudioAction.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct ma_device;

struct Audio : Component, Actionable<Action::Audio::Any> {
    Audio(ComponentArgs &&);
    ~Audio();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;
};
