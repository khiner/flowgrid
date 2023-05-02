#pragma once

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Field.h"

using Store = immer::map<StatePath, Primitive, StatePathHash>;
using TransientStore = immer::map_transient<StatePath, Primitive, StatePathHash>;

/**
Declare global read-only accessors for the complete, canonical application store instance `AppStore`.
 */
extern const Store &AppStore;

// Store setters
void Set(const Field::Base &, const Primitive &, TransientStore &);
void Set(const StoreEntries &, TransientStore &);
void Set(const Field::Entries &, TransientStore &);
void Set(const StatePath &, const vector<Primitive> &, TransientStore &);
void Set(const StatePath &, const vector<Primitive> &, Count row_count, TransientStore &); // For `SetMatrix` action.

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath = RootPath);

// TODO these are actually defined in `App.cpp`, because of circular dependencies.
template<IsPrimitive T>
struct Vector : Field::Base {
    using Base::Base;

    StatePath PathAt(const Count i) const;
    Count Size() const;
    T operator[](const Count i) const;
    void Set(const vector<T> &, TransientStore &) const;
    void Set(const vector<std::pair<int, T>> &, TransientStore &) const;

    void Update() override;

private:
    vector<T> Value;
};

// Vector of vectors. Inner vectors need not have the same length.
template<IsPrimitive T>
struct Vector2D : Field::Base {
    using Base::Base;

    StatePath PathAt(const Count i, const Count j) const;
    Count Size() const; // Number of outer vectors
    Count Size(Count i) const; // Size of inner vector at index `i`

    T operator()(Count i, Count j) const;
    void Set(const vector<vector<T>> &, TransientStore &) const;

    void Update() override;

private:
    vector<vector<T>> Value;
};

template<IsPrimitive T>
struct Matrix : Field::Base {
    using Base::Base;

    StatePath PathAt(const Count row, const Count col) const;
    Count Rows() const;
    Count Cols() const;
    T operator()(const Count row, const Count col) const;

    void Update() override;

private:
    Count RowCount, ColCount;
    vector<T> Data;
};
