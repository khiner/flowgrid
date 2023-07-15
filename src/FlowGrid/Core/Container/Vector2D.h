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
    void RenderValueTree(bool annotate, bool auto_select) const override;

    T operator()(u32 i, u32 j) const { return Value[i][j]; }

    StorePath PathAt(const u32 i, const u32 j) const { return Path / to_string(i) / to_string(j); }
    u32 Size() const { return Value.size(); }; // Number of outer vectors
    u32 Size(u32 i) const { return Value[i].size(); }; // Size of inner vector at index `i`

    void Set(const std::vector<std::vector<T>> &) const;
    void Set(u32 i, u32 j, const T &) const;
    void Resize(u32 size) const;
    void Resize(u32 i, u32 size) const;

private:
    std::vector<std::vector<T>> Value;
};
