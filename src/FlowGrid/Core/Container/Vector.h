#pragma once

#include "Core/Field/Field.h"
#include "VectorAction.h"

// todo make this a `TypedField<std::vector<T>>`.
template<IsPrimitive T> struct Vector : Field, Actionable<typename Action::Vector<T>::Any> {
    using Field::Field;

    // `ActionType` is a type alias in `Actionable`, but it is not accessible here.
    // `Actionable` is templated on `Action::Vector::Type<T>::type`, which is a dependent type (it depends on `T`),
    // and base class members that use dependent template types are not visible in subclasses at compile time.
    using typename Actionable<typename Action::Vector<T>::Any>::ActionType;

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const Action::Vector<T>::Set &a) { Set(a.value); },
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    auto begin() const { return Value.begin(); }
    auto end() const { return Value.end(); }

    StorePath PathAt(Count i) const { return Path / to_string(i); }
    Count Size() const { return Value.size(); }
    T operator[](size_t i) const { return Value[i]; }

    void Set(const std::vector<T> &) const;
    void Set(size_t i, const T &value) const;
    void Set(const std::vector<std::pair<int, T>> &) const;

    void RefreshValue() override;

private:
    std::vector<T> Value;
};
