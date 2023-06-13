#include "Vector.h"

#include <algorithm>

#include "Core/Store/Store.h"

void VectorBase::Apply(const Action::Vector::Any &action) {
    Visit(
        action,
        [](const Action::Vector::Set &a) { store::Set(a.path, a.value); },
    );
}

template<IsPrimitive T> size_t Vector<T>::IndexOf(const T &value) const { return std::find(Value.begin(), Value.end(), value) - Value.begin(); }

template<IsPrimitive T> void Vector<T>::Set(const std::vector<T> &values) const {
    Count i = 0;
    while (i < values.size()) {
        store::Set(PathAt(i), T(values[i])); // When T is a bool, an explicit cast seems to be needed?
        i++;
    }
    while (store::CountAt(PathAt(i))) {
        store::Erase(PathAt(i));
        i++;
    }
}

template<IsPrimitive T> void Vector<T>::Erase(const T &value) const {
    const size_t index = IndexOf(value);
    if (index != Value.size()) {
        store::Erase(PathAt(index));
        for (size_t i = index + 1; i < Value.size(); i++) {
            store::Set(PathAt(i - 1), Value[i]);
            store::Erase(PathAt(i));
        }
    }
}

template<IsPrimitive T> void Vector<T>::Append(const T &value) const {
    store::Set(PathAt(Value.size()), value);
}

template<IsPrimitive T> void Vector<T>::Set(const std::vector<std::pair<int, T>> &values) const {
    for (const auto &[i, value] : values) store::Set(PathAt(i), value);
}

template<IsPrimitive T> void Vector<T>::Update() {
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
