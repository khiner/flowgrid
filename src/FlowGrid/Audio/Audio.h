#pragma once

#include "AudioAction.h"
#include "Core/ActionProducerComponent.h"
#include "Faust/Faust.h"
#include "FlowGrid/TextEditor/TextBufferAction.h"
#include "Graph/AudioGraph.h"

struct Audio : ActionProducerComponent<Action::Combine<Action::Audio::Any, Action::AdjacencyList::Any, Navigable<ID>::ProducedActionType, Colors::ProducedActionType, Action::TextBuffer::Any>> {
    Audio(ArgsT &&, const FileDialog &);
    ~Audio();

    struct Style : Component {
        using Component::Component;

        void Render() const override;
    };

    const FileDialog &FileDialog;

    ProducerProp_(AudioGraph, Graph, "Audio graph");
    ProducerProp(Faust, Faust, FileDialog);
    Prop_(Style, Style, "Audio style");

private:
    void Render() const override;
};
