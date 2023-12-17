#pragma once

#include "Action/ActionProducer.h"
#include "Action/Actionable.h"
#include "ActionableComponentArgs.h"
#include "Component.h"

// `ActionType` is the type of actions that can be applied to the component.
template<typename ActionType, typename ProducedActionType = ActionType>
struct ActionableComponent : Component, Actionable<ActionType>, ActionProducer<ProducedActionType> {
    ActionableComponent(ActionableComponentArgs<ActionType> &&args)
        : Component(std::move(args.Args)), ActionProducer<ProducedActionType>(std::move(args.Q)) {}

    virtual ~ActionableComponent() = default;
};
