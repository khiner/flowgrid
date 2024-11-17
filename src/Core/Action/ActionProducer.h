#pragma once

#include <functional>
#include <variant>

template<typename T> struct ActionProducer {
    using ProducedActionType = T;
    using EnqueueFn = std::function<bool(ProducedActionType)>;

    ActionProducer(EnqueueFn &&owned_q) : Q(std::move(owned_q)) {}
    virtual ~ActionProducer() = default;

    // `SubProducer` supports action producers that only know about a subset action type (an action variant composed
    // only of members also in `ActionType`) to queue their actions into this superset-producer's queue.
    // An instance of `SubProducer<ActionSubType>` can be used as an `ActionProducer<ActionSubType>::EnqueueFn`.
    template<typename ActionSubType> struct SubProducer {
        SubProducer(const ActionProducer<ProducedActionType> &producer) : Producer(producer) {}

        bool operator()(ActionSubType &&action) {
            return std::visit([this](auto &&a) -> bool { return Producer.Q(std::move(a)); }, std::move(action));
        }

        const ActionProducer<ProducedActionType> &Producer;
    };

    EnqueueFn Q;
};
