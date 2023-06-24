#pragma once

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : Component, Actionable<Action::Audio::Any> {
    using Component::Component;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Init();
    void Uninit();
    void Update();

    Prop(AudioDevice, Device);
    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;

private:
    bool NeedsRestart() const;
};
