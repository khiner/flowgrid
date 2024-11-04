#pragma once

#include <functional>

#include "CoreAction.h"

struct CoreActionProducer {
    using EnqueueFn = std::function<bool(Action::Core::Any &&)>;

    CoreActionProducer(EnqueueFn q) : Q(std::move(q)) {}

    bool operator()(auto &&action) const { return Q(std::move(action)); }

private:
    EnqueueFn Q;
};
