#pragma once

#include <memory>

#include "concurrentqueue.h"

#include "Core/Action/ActionMoment.h"

template<typename ActionType> struct ActionQueue {
    using ProducerToken = moodycamel::ProducerToken;
    using ConsumerToken = moodycamel::ConsumerToken;

    ProducerToken CreateProducerToken() { return ProducerToken{Queue}; }
    ConsumerToken CreateConsumerToken() { return ConsumerToken{Queue}; }

    bool Enqueue(const ProducerToken &ptok, ActionMoment<ActionType> &&action_moment) { return Queue.enqueue(ptok, std::move(action_moment)); }
    bool Enqueue(const ProducerToken &ptok, ActionType &&action) { return Enqueue(ptok, {std::move(action), Clock::now()}); }
    bool TryDequeue(ConsumerToken &ctok, ActionMoment<ActionType> &action_moment) { return Queue.try_dequeue(ctok, action_moment); }

private:
    using QueueType = moodycamel::ConcurrentQueue<ActionMoment<ActionType>, moodycamel::ConcurrentQueueDefaultTraits>;
    QueueType Queue;
};
