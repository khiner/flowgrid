#pragma once

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : Component, Drawable {
    using Component::Component;

    void Apply(const Action::Audio::Any &) const;
    bool CanApply(const Action::Audio::Any &) const;

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
