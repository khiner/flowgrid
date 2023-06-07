#pragma once

#include "Field.h"

namespace Stateful::Field {
template<IsPrimitive T>
struct Vector : Base {
    using Base::Base;

    StorePath PathAt(const Count i) const { return Path / to_string(i); }
    Count Size() const { return Value.size(); }
    T operator[](const Count i) const { return Value[i]; }

    void Set(const std::vector<T> &) const;
    void Set(const std::vector<std::pair<int, T>> &) const;

    void Update() override;

private:
    std::vector<T> Value;
};
} // namespace Stateful::Field
