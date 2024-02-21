#pragma once

#include "AudioAction.h"
#include "Core/ActionableComponent.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"
#include "Project/TextEditor/TextBufferAction.h"

struct Audio : ActionableComponent<Action::Audio::Any, Action::Combine<Action::Audio::Any, Action::AdjacencyList::Any, Navigable<ID>::ProducedActionType, Colors::ProducedActionType, Action::TextBuffer::Any>> {
    Audio(ArgsT &&, const FileDialog &);
    ~Audio();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

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
