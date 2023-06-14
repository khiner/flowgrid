#include "Vector.h"

#include <algorithm>

#include "Core/Store/Store.h"

void VectorBase::ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Vector::Set &a) { store::Set(a.path, a.value); },
    );
}

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

template<IsPrimitive T> void Vector<T>::Set(size_t i, const T &value) const {
    store::Set(PathAt(i), value);
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
