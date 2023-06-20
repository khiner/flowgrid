#include "Matrix.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> void Matrix<T>::Set(const std::vector<T> &value, Count row_count) const {
    assert(value.size() % row_count == 0);

    const Count col_count = value.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            Set(row, col, value[row * col_count + col]);
            col++;
        }
        while (store::CountAt(PathAt(row, col))) {
            store::Erase(PathAt(row, col));
            col++;
        }
        row++;
    }

    while (store::CountAt(PathAt(row, 0))) {
        Count col = 0;
        while (store::CountAt(PathAt(row, col))) {
            store::Erase(PathAt(row, col));
            col++;
        }
        row++;
    }
}

template<IsPrimitive T> void Matrix<T>::Set_(const std::vector<T> &value, Count row_count) {
    Value = value;
    RowCount = row_count;
    Set(value, row_count);
}

template<IsPrimitive T> void Matrix<T>::Set(Count row, Count col, const T &value) const {
    store::Set(PathAt(row, col), value);
}

template<IsPrimitive T> void Matrix<T>::RefreshValue() {
    Count row_count = 0, col_count = 0;
    while (store::CountAt(PathAt(row_count, 0))) { row_count++; }
    while (store::CountAt(PathAt(row_count - 1, col_count))) { col_count++; }
    RowCount = row_count;
    ColCount = col_count;
    Value.resize(RowCount * ColCount);

    for (Count row = 0; row < RowCount; row++) {
        for (Count col = 0; col < ColCount; col++) {
            Value[row * ColCount + col] = std::get<T>(store::Get(PathAt(row, col)));
        }
    }
}

// Explicit instantiations.
template struct Matrix<bool>;
