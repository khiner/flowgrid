#pragma once

#include <memory>

#include "Core/Action/ActionMoment.h"

// Forward declaration of BlockingConcurrentQueue
namespace moodycamel {
struct ConcurrentQueueDefaultTraits;
template<typename T, typename Traits> class BlockingConcurrentQueue;
} // namespace moodycamel

template<typename ActionType> struct ActionQueue {
    ActionQueue();
    ~ActionQueue();

    bool Enqueue(ActionMoment<ActionType> &&);
    bool Enqueue(ActionType &&);
    bool TryDequeue(ActionMoment<ActionType> &);

private:
    using QueueType = moodycamel::BlockingConcurrentQueue<ActionMoment<ActionType>, moodycamel::ConcurrentQueueDefaultTraits>;
    std::unique_ptr<QueueType> Queue;
};
