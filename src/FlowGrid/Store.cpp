#include "Store.h"

#include <immer/algorithm.hpp>

// Transient modifiers
void Set(const Field::Base &field, const Primitive &value, TransientStore &store) { store.set(field.Path, value); }
void Set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value] : values) store.set(path, value);
}
void Set(const Field::Entries &values, TransientStore &store) {
    for (const auto &[field, value] : values) store.set(field.Path, value);
}
void Set(const StatePath &path, const vector<Primitive> &values, TransientStore &store) {
    Count i = 0;
    while (i < values.size()) {
        store.set(path / to_string(i), values[i]);
        i++;
    }

    while (store.count(path / to_string(i))) store.erase(path / to_string(i++));
}
void Set(const StatePath &path, const vector<Primitive> &data, const Count row_count, TransientStore &store) {
    assert(data.size() % row_count == 0);
    const Count col_count = data.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            store.set(path / to_string(row) / to_string(col), data[row * col_count + col]);
            col++;
        }
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }

    while (store.count(path / to_string(row) / to_string(0))) {
        Count col = 0;
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(BasePath)] = {AddOp, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(BasePath)] = {RemoveOp, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(BasePath)] = {ReplaceOp, new_element.second, old_element.second};
        }
    );

    return {ops, BasePath};
}
