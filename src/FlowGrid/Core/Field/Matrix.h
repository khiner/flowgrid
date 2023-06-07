#pragma once

#include "Field.h"

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
