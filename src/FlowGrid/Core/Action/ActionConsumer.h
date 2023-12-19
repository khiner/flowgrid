#pragma once

#include "ActionQueue.h"

template<typename ActionType> struct ActionConsumer {
    ActionConsumer(ActionQueue<ActionType> &queue) : Queue(queue) {}
    virtual ~ActionConsumer() = default;

    inline bool Q(ActionMoment<ActionType> &&action_moment) const { return Queue.Enqueue(std::move(action_moment)); }
    inline bool Q(ActionType &&action) const { return Q({std::move(action), Clock::now()}); }
    inline bool DQ(ActionMoment<ActionType> &action_moment) const { return Queue.TryDequeue(action_moment); }

    // `SubConsumer` supports action producers that only know about a subset action type (an action variant composed
    // only of members also in `ActionType`) to queue their actions into this superset-consumer's `Queue`.
    // An instance of `SubConsumer<ActionSubType>` can be used as an `ActionProducer<ActionSubType>::EnqueueFn`.
    template<typename ActionSubType> struct SubConsumer {
        SubConsumer(const ActionConsumer<ActionType> *queuer) : Queuer(queuer) {}

        bool operator()(ActionSubType &&action) {
            return std::visit([this](auto &&a) -> bool { return Queuer->Q(std::move(a)); }, std::move(action));
        }

        const ActionConsumer<ActionType> *Queuer;
    };

    template<typename SubActionType> SubConsumer<SubActionType> CreateConsumer() const {
        return SubConsumer<SubActionType>(this);
    }

private:
    ActionQueue<ActionType> &Queue;
};
