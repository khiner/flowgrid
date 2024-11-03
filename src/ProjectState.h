#pragma once

#include "FlowGrid.h"
#include "ProjectCore.h"

/**
Fully describes the project state at any point in time.
It holds both core and application-specific state.
It's a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
*/
struct ProjectState : Component,
                      ActionableProducer<
                          Action::Combine<::ProjectCore::ActionType, ::FlowGrid::ActionType>,
                          Action::Combine<::ProjectCore::ProducedActionType, ::FlowGrid::ProducedActionType>> {
    ProjectState(Store &, ActionProducer::Enqueue, const ::ProjectContext &);
    ~ProjectState();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    // Overriding to not draw root submenu.
    void DrawWindowsMenu() const override {
        for (const auto *c : Children) c->DrawWindowsMenu();
    }

    ProducerProp(ProjectCore, Core);
    ProducerProp(FlowGrid, FlowGrid);

protected:
    void Render() const override;
};
