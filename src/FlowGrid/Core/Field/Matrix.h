#pragma once

#include "Field.h"
#include "MatrixAction.h"

struct MatrixBase {
    struct ActionHandler : Actionable<Action::Matrix::Any> {
        void Apply(const ActionType &) const override;
        bool CanApply(const ActionType &) const override { return true; }
    };

    inline static ActionHandler ActionHandler;
};

template<IsPrimitive T> struct Matrix : Field {
    using Field::Field;

    StorePath PathAt(const Count row, const Count col) const { return Path / to_string(row) / to_string(col); }
    Count Rows() const { return RowCount; }
    Count Cols() const { return ColCount; }

    T operator()(const Count row, const Count col) const { return Data[row * ColCount + col]; }

    void Update() override;

private:
    Count RowCount, ColCount;
    std::vector<T> Data;
};
