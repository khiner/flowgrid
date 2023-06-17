#include "Matrix.h"

#include "Core/Store/Store.h"

#include <range/v3/range/conversion.hpp>

void MatrixBase::ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Matrix::Set &a) { store::Set(a.path, a.value, a.row_count); },
    );
}

template<IsPrimitive T> void Matrix<T>::Update() {
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

template<IsPrimitive T> void Matrix<T>::Set(const std::vector<T> &value, const Count row_count) {
    Value = value;

    const std::vector<Primitive> primitives = value | std::views::transform([](const T &v) { return Primitive(v); }) | ranges::to<std::vector>();
    store::Set(Path, primitives, row_count);
}

// Explicit instantiations.
template struct Matrix<bool>;
