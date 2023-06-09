#pragma once

#include "Field.h"

struct VectorBase : Field {
    using Field::Field;

    static void Apply(const Action::Vector::Any &);
    static bool CanApply(const Action::Vector::Any &) { return true; }
    StorePath PathAt(const Count i) const { return Path / to_string(i); }
};

template<IsPrimitive T>
struct Vector : VectorBase {
    using VectorBase::VectorBase;

    Count Size() const { return Value.size(); }
    T operator[](const Count i) const { return Value[i]; }

    void Set(const std::vector<T> &) const;
    void Set(const std::vector<std::pair<int, T>> &) const;

    void Update() override;

private:
    std::vector<T> Value;
};
