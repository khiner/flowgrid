#pragma once

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Core/Action/Actionable.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : Component, Actionable<Action::Audio::Any>, Field::ChangeListener {
    Audio(ComponentArgs &&);
    ~Audio();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void OnFieldChanged() override;

    void Init();
    void Uninit();

    Prop(AudioDevice, Device);
    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);

protected:
    void Render() const override;
};
