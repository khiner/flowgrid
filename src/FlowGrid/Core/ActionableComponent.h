#pragma once

#include "Action/ActionProducer.h"
#include "Action/Actionable.h"
#include "Component.h"
#include "ProducerComponentArgs.h"

// `ActionType` is the type of action that can be _applied_ to the component.
template<typename ActionType, typename ProducedActionType = ActionType>
struct ActionableComponent : Component, Actionable<ActionType>, ActionProducer<ProducedActionType> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    ActionableComponent(ArgsT &&args)
        : Component(std::move(args.Args)), ActionProducer<ProducedActionType>(std::move(args.Q)) {}

    virtual ~ActionableComponent() = default;
};
