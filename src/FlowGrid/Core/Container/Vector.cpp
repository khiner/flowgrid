#include "Vector.h"

#include <range/v3/range/conversion.hpp>

#include "Core/Store/Store.h"

template<IsPrimitive T> void Vector<T>::Set(const std::vector<T> &value) const {
    const std::vector<Primitive> primitives = value | std::views::transform([](const T &v) { return Primitive(v); }) | ranges::to<std::vector>();
    store::Set(Path, primitives);
}

template<IsPrimitive T> void Vector<T>::Set(size_t i, const T &value) const {
    store::Set(PathAt(i), value);
}

template<IsPrimitive T> void Vector<T>::Set(const std::vector<std::pair<int, T>> &values) const {
    for (const auto &[i, value] : values) store::Set(PathAt(i), value);
}

template<IsPrimitive T> void Vector<T>::RefreshValue() {
    Count i = 0;
    while (store::CountAt(PathAt(i))) {
        const T value = std::get<T>(store::Get(PathAt(i)));
        if (Value.size() == i) Value.push_back(value);
        else Value[i] = value;
        i++;
    }
    Value.resize(i);
}

// Explicit instantiations.
template struct Vector<bool>;
template struct Vector<int>;
template struct Vector<U32>;
template struct Vector<float>;
