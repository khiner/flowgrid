#include "ActionQueue.h"

#include "blockingconcurrentqueue.h"

template<typename ActionType>
ActionQueue<ActionType>::ActionQueue()
    : Queue(std::make_unique<moodycamel::BlockingConcurrentQueue<ActionMoment<ActionType>>>()) {}

template<typename ActionType>
ActionQueue<ActionType>::~ActionQueue() = default;

template<typename ActionType>
bool ActionQueue<ActionType>::Enqueue(ActionMoment<ActionType> &&action_moment) {
    return Queue->enqueue(std::move(action_moment));
}

template<typename ActionType>
bool ActionQueue<ActionType>::Enqueue(ActionType &&action) {
    return Queue->enqueue({std::move(action), Clock::now()});
}

template<typename ActionType>
bool ActionQueue<ActionType>::TryDequeue(ActionMoment<ActionType> &action_moment) {
    return Queue->try_dequeue(action_moment);
}

#include "Core/Action/Actions.h"

// Explicit instantiation.
template struct ActionQueue<Action::Any>;
