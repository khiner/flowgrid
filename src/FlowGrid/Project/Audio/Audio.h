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

    struct Style : Component {
        using Component::Component;

        void Render() const override;
    };

    Prop_(AudioGraph, Graph, "Audio graph");
    Prop(Faust, Faust);
    Prop_(Style, Style, "Audio style");
};
