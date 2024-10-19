#pragma once

#include "Action/ActionProducer.h"
#include "Component.h"
#include "ProducerComponentArgs.h"

template<typename ProducedActionType>
struct ActionProducerComponent : Component, ActionProducer<ProducedActionType> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    ActionProducerComponent(ArgsT &&args)
        : Component(std::move(args.Args)), ActionProducer<ProducedActionType>(std::move(args.Q)) {}

    virtual ~ActionProducerComponent() = default;
};
