#pragma once

#include "Action/ActionableProducer.h"
#include "Component.h"
#include "ProducerComponentArgs.h"

// `ActionType` is the type of action that can be _applied_ to the component.
template<typename ActionType, typename ProducedActionType = ActionType>
struct ActionableComponent : Component, ActionableProducer<ActionType, ProducedActionType> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    ActionableComponent(ArgsT &&args)
        : Component(std::move(args.Args)), ActionableProducer<ActionType, ProducedActionType>(std::move(args.Q)) {}

    virtual ~ActionableComponent() = default;
};
