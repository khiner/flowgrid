#pragma once

#include <functional>
#include <variant>

template<typename T> struct ActionProducer {
    using ProducedActionType = T;
    using EnqueueFn = std::function<bool(ProducedActionType &&)>;

    // Store either an owned `EnqueueFn` or a reference to one.
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
        return std::visit([&action](auto &&q) -> bool { return q(action); }, q);
    }

protected:
    Enqueue q; // Protected so derived classes can pass it to child producers.
};
