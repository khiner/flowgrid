#pragma once

#include "Field.h"
#include "MatrixAction.h"

struct MatrixBase : Field {
    using Field::Field;

    StorePath PathAt(const Count row, const Count col) const { return Path / to_string(row) / to_string(col); }

    Count Rows() const { return RowCount; }
    Count Cols() const { return ColCount; }

    struct ActionHandler {
        void Apply(const Action::Matrix::Any &);
        bool CanApply(const Action::Matrix::Any &) { return true; }
    };

    inline static ActionHandler ActionHandler;

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
