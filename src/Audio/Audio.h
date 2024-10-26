#pragma once

#include "AudioAction.h"
#include "Core/ActionProducerComponent.h"
#include "Core/TextEditor/TextBufferAction.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : ActionProducerComponent<Action::Combine<Action::Audio::Any, Action::AdjacencyList::Any, Navigable<ID>::ProducedActionType, Colors::ProducedActionType, Action::TextBuffer::Any>> {
    Audio(ArgsT &&);
    ~Audio();

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
