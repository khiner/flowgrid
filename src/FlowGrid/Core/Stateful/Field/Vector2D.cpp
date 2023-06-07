#include "Vector2D.h"

#include "Core/Store/Store.h"

namespace Stateful::Field {
template<IsPrimitive T> void Vector2D<T>::Set(const std::vector<std::vector<T>> &values) const {
    Count i = 0;
    while (i < values.size()) {
        Count j = 0;
        while (j < values[i].size()) {
            store::Set(PathAt(i, j), T(values[i][j]));
            j++;
        }
        while (store::CountAt(PathAt(i, j))) store::Erase(PathAt(i, j++));
        i++;
    }

    while (store::CountAt(PathAt(i, 0))) {
        Count j = 0;
        while (store::CountAt(PathAt(i, j))) store::Erase(PathAt(i, j++));
        i++;
    }
}

template<IsPrimitive T> void Vector2D<T>::Update() {
    Count i = 0;
    while (store::CountAt(PathAt(i, 0))) {
        if (Value.size() == i) Value.push_back({});
        Count j = 0;
        while (store::CountAt(PathAt(i, j))) {
            const T value = std::get<T>(store::Get(PathAt(i, j)));
            if (Value[i].size() == j) Value[i].push_back(value);
            else Value[i][j] = value;
            j++;
        }
        Value[i].resize(j);
        i++;
    }
    Value.resize(i);
}

// Explicit instantiations.
template struct Vector2D<bool>;
template struct Vector2D<int>;
template struct Vector2D<U32>;
template struct Vector2D<float>;
} // namespace Stateful::Field
