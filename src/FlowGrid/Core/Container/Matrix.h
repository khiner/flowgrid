#pragma once

#include "Core/Field/Field.h"
#include "MatrixAction.h"

// todo make this a `TypedField<MatrixData>`, where `MatrixData` holds RowCount, ColCount, Value.
template<IsPrimitive T> struct Matrix : Field, Actionable<typename Action::Matrix<T>::Any> {
    using Field::Field;
    using typename Actionable<typename Action::Matrix<T>::Any>::ActionType; // See note in `Vector.h`.

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const Action::Matrix<T>::Set &a) { Set(a.value, a.row_count); },
            [this](const Action::Matrix<T>::SetValue &a) { Set(a.row, a.col, a.value); },
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    void Set(const std::vector<T> &, Count row_count) const;
    void Set_(const std::vector<T> &, Count row_count);

    void Set(Count row, Count col, const T &) const;

    StorePath PathAt(Count row, Count col) const { return Path / to_string(row) / to_string(col); }

    Count Rows() const { return RowCount; }
    Count Cols() const { return ColCount; }

    T operator()(Count row, Count col) const { return Value[row * ColCount + col]; }

    void RefreshValue() override;

private:
    Count RowCount, ColCount;
    std::vector<T> Value;
};
