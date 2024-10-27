#pragma once

#include "FlowGrid.h"
#include "ProjectCore.h"

struct ProjectContext;

/**
Fully describes the project state at any point in time.
It holds both core and application-specific state.
It's a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
*/
struct ProjectState : Component, ActionableProducer<Action::State::Any, Action::Any> {
    ProjectState(Store &, ActionProducer::Enqueue, const ::ProjectContext &);
    ~ProjectState();

    const PrimitiveActionQueuer PrimitiveQ{CreateProducer<PrimitiveActionQueuer::ActionType>()};

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    ProducerProp(ProjectCore, Core);
    ProducerProp(FlowGrid, FlowGrid);

protected:
    void Render() const override;
};
