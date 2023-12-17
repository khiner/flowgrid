#pragma once

#include "Action/Actionable.h"
#include "Action/ActionProducer.h"
#include "ComponentArgs.h"

template<typename ActionType> struct ActionableComponentArgs {
    ComponentArgs &&Args;
    ActionProducer<ActionType>::Enqueue &&Q;
};
