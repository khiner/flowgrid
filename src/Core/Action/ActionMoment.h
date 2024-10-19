#pragma once

#include "Core/Helper/Time.h"

// An `ActionMoment` is an action paired with its queue time.
template<typename ActionType> struct ActionMoment {
    ActionType Action;
    TimePoint QueueTime;
};
