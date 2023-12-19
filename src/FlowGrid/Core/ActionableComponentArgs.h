#pragma once

#include "Action/Actionable.h"
#include "Action/ActionProducer.h"
#include "ComponentArgs.h"

template<typename ActionType, typename ProducedActionType = ActionType> struct ActionableComponentArgs {
    ComponentArgs &&Args;
    ActionProducer<ProducedActionType>::EnqueueFn &&Q;
};
