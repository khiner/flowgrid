#pragma once

#include "Action/ActionProducer.h"
#include "ComponentArgs.h"

template<typename ProducedActionType> struct ProducerComponentArgs {
    ComponentArgs &&Args;
    ActionProducer<ProducedActionType>::EnqueueFn Q;
};
