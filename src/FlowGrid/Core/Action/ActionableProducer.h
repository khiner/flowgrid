#pragma once

#include "ActionProducer.h"
#include "Actionable.h"

template<typename ActionType, typename ProducedActionType = ActionType>
struct ActionableProducer : Actionable<ActionType>, ActionProducer<ProducedActionType> {
    using ActionProducer<ProducedActionType>::ActionProducer;
};
