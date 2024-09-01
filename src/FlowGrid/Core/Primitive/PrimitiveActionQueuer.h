#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Container/Vec2Action.h"
#include "PrimitiveAction.h"

struct PrimitiveActionQueuer {
    using ProducedActionType = Action::Combine<Action::Primitive::Any, Action::Vec2::Any>;
    using EnqueueFn = ActionProducer<ProducedActionType>::EnqueueFn;

    PrimitiveActionQueuer(EnqueueFn q) : Q(std::move(q)) {}

    EnqueueFn Q;
};
