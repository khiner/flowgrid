#include "Vector.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> void Vector<T>::Set(const std::vector<T> &value) const {
    Count i = 0;
    while (i < value.size()) {
        Set(i, value[i]);
        i++;
    }
    Resize(i);
}

template<IsPrimitive T> void Vector<T>::Resize(Count size) const {
    Count i = size;
    while (store::CountAt(PathAt(i))) {
        store::Erase(PathAt(i));
        i++;
    }
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