#pragma once

#include "Core/Action/CoreAction.h"
#include "FlowGridAction.h"

#include "Audio/Audio.h"

/**
`FlowGrid` fully describes the application-specific (non-core, non-project) state at any point in time.
*/
struct FlowGrid : ActionableComponent<Action::FlowGrid::Any, Action::Combine<Action::Core::Any, Action::FlowGrid::Any>> {
    using ActionableComponent::ActionableComponent;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    ProducerProp(Audio, Audio);
};
