#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Container/Vec2Action.h"
#include "PrimitiveAction.h"

struct PrimitiveActionQueuer {
    using ActionType = Action::Combine<Action::Primitive::Any, Action::Vec2::Any>;
    using EnqueueFn = ActionProducer<ActionType>::EnqueueFn;

    PrimitiveActionQueuer(EnqueueFn q) : Q(std::move(q)) {}

    void operator()(auto &&action) const { Q(std::move(action)); }

private:
    EnqueueFn Q;
};
