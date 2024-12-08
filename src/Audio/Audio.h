#pragma once

#include "AudioAction.h"
#include "Core/ActionableComponent.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : ActionableComponent<Action::Audio::Any> {
    Audio(ArgsT &&);
    ~Audio();

    void Apply(TransientStore &, const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Dock(ID *) const override;

    struct Style : Component {
        using Component::Component;

        void Render() const override;
    };

    ProducerProp_(AudioGraph, Graph, "Audio graph");
    ProducerProp(Faust, Faust);
    Prop_(Style, Style, "Audio style");

private:
    void Render() const override;
};
