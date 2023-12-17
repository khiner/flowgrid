#pragma once

#include "ActionQueue.h"
#include "Helper/Variant.h"

template<typename ActionType> struct ActionConsumer {
    ActionConsumer(ActionQueue<ActionType> &queue) : Queue(queue) {}
    virtual ~ActionConsumer() = default;

    inline bool Q(ActionMoment<ActionType> &&action_moment) const { return Queue.Enqueue(std::move(action_moment)); }
    inline bool Q(ActionType &&action) const { return Q({std::move(action), Clock::now()}); }
    inline bool DQ(ActionMoment<ActionType> &action_moment) const { return Queue.TryDequeue(action_moment); }

    // `SubConsumer` supports action producers that only know about a subset action type (an action variant composed
    // only of members also in `ActionType`) to queue their actions into this superset-consumer's `Queue`.
    // An instance of `SubConsumer<ActionSubType>` can be used as an `Actionable<ActionSubType>::Enqueue` function.
    template<typename ActionSubType> struct SubConsumer {
        SubConsumer(const ActionConsumer<ActionType> *queuer) : Queuer(queuer) {}

        bool operator()(ActionSubType &&action) {
            return Visit(std::move(action), [this](auto &&a) { return Queuer->Q(std::move(a)); });
        }

        const ActionConsumer<ActionType> *Queuer;
    };

    template<typename ActionableType> SubConsumer<typename ActionableType::ActionType> CreateConsumer() const {
        return SubConsumer<typename ActionableType::ActionType>(this);
    }

private:
    ActionQueue<ActionType> &Queue;
};
