#pragma once

#include "blockingconcurrentqueue.h"

#include "Core/Action/ActionMoment.h"

template<typename ActionType> struct ActionQueue {
    bool Enqueue(ActionMoment<ActionType> &&action_moment) {
        return Queue.enqueue(std::move(action_moment));
    }
    bool Enqueue(ActionType &&action) {
        return Queue.enqueue({std::move(action), Clock::now()});
    }
    bool TryDequeue(ActionMoment<ActionType> &action_moment) {
        return Queue.try_dequeue(action_moment);
    }

private:
    moodycamel::BlockingConcurrentQueue<ActionMoment<ActionType>> Queue{};
};
