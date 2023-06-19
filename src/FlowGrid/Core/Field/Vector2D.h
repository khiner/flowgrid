#pragma once

#include "Field.h"

// Vector of vectors. Inner vectors need not have the same length.
template<IsPrimitive T> struct Vector2D : Field {
    using Field::Field;

    StorePath PathAt(const Count i, const Count j) const { return Path / to_string(i) / to_string(j); }
    Count Size() const { return Value.size(); }; // Number of outer vectors
    Count Size(Count i) const { return Value[i].size(); }; // Size of inner vector at index `i`
    T operator()(Count i, Count j) const { return Value[i][j]; }

    void Set(const std::vector<std::vector<T>> &) const;

    void RefreshValue() override;

private:
    std::vector<std::vector<T>> Value;
};
