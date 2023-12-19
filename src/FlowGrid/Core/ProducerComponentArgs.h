#pragma once

#include "Action/ActionProducer.h"
#include "Action/Actionable.h"
#include "ComponentArgs.h"

template<typename ProducedActionType> struct ProducerComponentArgs {
    ComponentArgs &&Args;
    ActionProducer<ProducedActionType>::Enqueue Q;
};
