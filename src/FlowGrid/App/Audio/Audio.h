#pragma once

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : Component, Drawable, Actionable<Action::Audio::Any> {
    using Component::Component;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Init() const;
    void Uninit() const;
    void Update() const;
    bool NeedsRestart() const;

    Prop(AudioDevice, Device);
    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;
};
