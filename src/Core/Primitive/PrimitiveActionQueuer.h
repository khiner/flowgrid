#pragma once

#include <functional>

#include "Core/Container/ContainerAction.h"
#include "PrimitiveAction.h"

struct PrimitiveActionQueuer {
    using ActionType = Action::Combine<Action::Primitive::Any, Action::Container::Any>;
    using EnqueueFn = std::function<bool(ActionType &&)>;

    PrimitiveActionQueuer(EnqueueFn q) : Q(std::move(q)) {}

    void operator()(auto &&action) const { Q(std::move(action)); }

private:
    EnqueueFn Q;
};
