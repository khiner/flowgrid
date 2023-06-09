#include "Matrix.h"

#include "Core/Store/Store.h"

void MatrixBase::Apply(const Action::Matrix::Any &action) {
    Match(
        action,
        [](const Action::Matrix::Set &a) { store::Set(a.path, a.data, a.row_count); },
    );
}

template<IsPrimitive T> void Matrix<T>::Update() {
    Count row_count = 0, col_count = 0;
    while (store::CountAt(PathAt(row_count, 0))) { row_count++; }
    while (store::CountAt(PathAt(row_count - 1, col_count))) { col_count++; }
    RowCount = row_count;
    ColCount = col_count;
    Data.resize(RowCount * ColCount);

    for (Count row = 0; row < RowCount; row++) {
        for (Count col = 0; col < ColCount; col++) {
            Data[row * ColCount + col] = std::get<T>(store::Get(PathAt(row, col)));
        }
    }
}

// Explicit instantiations.
template struct Matrix<bool>;
