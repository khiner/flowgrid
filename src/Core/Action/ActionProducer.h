#pragma once

#include <functional>
#include <variant>

// todo use `Emit` instead of `Q` for producers.
template<typename T> struct ActionProducer {
    using ProducedActionType = T;
    using EnqueueFn = std::function<bool(ProducedActionType &&)>;

    // Either an owned `EnqueueFn` or a const-ref to one.
    using Enqueue = std::variant<EnqueueFn, std::reference_wrapper<const EnqueueFn>>;

    ActionProducer(EnqueueFn &&owned_q) : q(std::move(owned_q)) {}
    ActionProducer(const EnqueueFn &external_q) : q(std::cref(external_q)) {}
    ActionProducer(Enqueue external_q) : q(std::move(external_q)) {}

    virtual ~ActionProducer() = default;

    // Call either the owned or referenced `Enqueue`, on either a rvalue or lvalue action.
    bool Q(ProducedActionType &&action) const {
        return std::visit([&action](auto &&q) -> bool { return q(std::move(action)); }, q);
    }
    bool Q(const ProducedActionType &action) const {
        return std::visit([&action](auto &&q) -> bool { return q(ProducedActionType{action}); }, q);
    }

    // `SubProducer` supports action producers that only know about a subset action type (an action variant composed
    // only of members also in `ActionType`) to queue their actions into this superset-producer's `q`.
    // An instance of `SubProducer<ActionSubType>` can be used as an `ActionProducer<ActionSubType>::EnqueueFn`.
    template<typename ActionSubType> struct SubProducer {
        SubProducer(const ActionProducer<ProducedActionType> *queuer) : Queuer(queuer) {}

        bool operator()(ActionSubType &&action) {
            return std::visit([this](auto &&a) -> bool { return Queuer->Q(std::move(a)); }, std::move(action));
        }

        const ActionProducer<ProducedActionType> *Queuer;
    };

    template<typename SubActionType> SubProducer<SubActionType> CreateProducer() const {
        return SubProducer<SubActionType>(this);
    }

protected:
    Enqueue q; // Protected to support passing to child producers.
};
