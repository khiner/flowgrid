#pragma once

#include "Core/Field/Field.h"
#include "Vector2DAction.h"

// Vector of vectors. Inner vectors may have different sizes.
template<IsPrimitive T> struct Vector2D : Field, Actionable<typename Action::Vector2D<T>::Any> {
    using Field::Field;
    using typename Actionable<typename Action::Vector2D<T>::Any>::ActionType; // See note in `Vector.h`.

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const Action::Vector2D<T>::Set &a) { Set(a.value); },
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    void RefreshValue() override;
    void RenderValueTree(ValueTreeLabelMode, bool auto_select) const override;

    T operator()(Count i, Count j) const { return Value[i][j]; }

    StorePath PathAt(const Count i, const Count j) const { return Path / to_string(i) / to_string(j); }
    Count Size() const { return Value.size(); }; // Number of outer vectors
    Count Size(Count i) const { return Value[i].size(); }; // Size of inner vector at index `i`

    void Set(const std::vector<std::vector<T>> &) const;
    void Set(Count i, Count j, const T &) const;
    void Resize(Count size) const;
    void Resize(Count i, Count size) const;

private:
    std::vector<std::vector<T>> Value;
};
