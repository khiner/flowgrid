#pragma once

#include "Field.h"

struct MatrixBase : Field {
    using Field::Field;

    static void Apply(const Action::Matrix::Any &);
    static bool CanApply(const Action::Matrix::Any &) { return true; }
    StorePath PathAt(const Count row, const Count col) const { return Path / to_string(row) / to_string(col); }

    Count Rows() const { return RowCount; }
    Count Cols() const { return ColCount; }

protected:
    Count RowCount, ColCount;
};

template<IsPrimitive T> struct Matrix : MatrixBase {
    using MatrixBase::MatrixBase;

    T operator()(const Count row, const Count col) const { return Data[row * ColCount + col]; }

    void Update() override;

private:
    std::vector<T> Data;
};
