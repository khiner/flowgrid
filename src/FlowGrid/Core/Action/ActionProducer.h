#pragma once

#include <functional>

template<typename T> struct ActionProducer {
    using ProduceActionType = T;
    using Enqueue = std::function<bool(ProduceActionType &&)>;

    ActionProducer(Enqueue &&q) : q{std::move(q)} {}
    virtual ~ActionProducer() = default;

    Enqueue q{};
};
