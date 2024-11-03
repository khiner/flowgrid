#pragma once

#include "Audio/Audio.h"
#include "FlowGridAction.h"

/**
`FlowGrid` fully describes the application-specific (non-core, non-project) state at any point in time.
*/
struct FlowGrid : ActionableComponent<Action::FlowGrid::Any, Audio::ProducedActionType> {
    using ActionableComponent::ActionableComponent;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void FocusDefault() const override;

    ProducerProp(Audio, Audio);
};
