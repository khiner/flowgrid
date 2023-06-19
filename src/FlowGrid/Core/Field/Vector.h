#pragma once

#include "Field.h"
#include "VectorAction.h"

struct VectorBase {
    struct ActionHandler : Actionable<Action::Vector::Any> {
        void Apply(const ActionType &) const override;
        bool CanApply(const ActionType &) const override { return true; }
    };

    inline static ActionHandler ActionHandler;
};

template<IsPrimitive T>
struct Vector : Field {
    using Field::Field;

    auto begin() const { return Value.begin(); }
    auto end() const { return Value.end(); }

    StorePath PathAt(const Count i) const { return Path / to_string(i); }
    Count Size() const { return Value.size(); }
    T operator[](size_t i) const { return Value[i]; }

    void Set(const std::vector<T> &) const;
    void Set(size_t i, const T &value) const;
    void Set(const std::vector<std::pair<int, T>> &) const;

    void RefreshValue() override;

private:
    std::vector<T> Value;
};
