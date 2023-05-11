#include "Store.h"

#include "immer/algorithm.hpp"
#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include <range/v3/core.hpp>
#include <range/v3/view/map.hpp>

#include "Action/Action.h"

TransientStore InitStore{};
Store ApplicationStore{};
const Store &AppStore = ApplicationStore;

namespace store {
void OnApplicationStateInitialized() {
    ApplicationStore = InitStore.persistent(); // Create the global canonical store, initially containing the full application state constructed by `State`.
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
    // Ensure all store values set during initialization are reflected in cached field/collection values.
    for (auto *field : ranges::views::values(Field::Base::WithPath)) field->Update();
}

Primitive Get(const StorePath &path) { return InitStore.empty() ? AppStore.at(path) : InitStore.at(path); }

Patch CreatePatch(const Store &before, const Store &after, const StorePath &BasePath) {
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

void ApplyPatch(const Patch &patch, TransientStore &store) {
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        if (op.Op == AddOp || op.Op == ReplaceOp) store.set(path, *op.Value);
        else if (op.Op == RemoveOp) store.erase(path);
    }
}

// Transient modifiers
void Set(const StorePath &path, const Primitive &value, TransientStore &store) { store.set(path, value); }
void Set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value] : values) store.set(path, value);
}
void Set(const StorePath &path, const vector<Primitive> &values, TransientStore &store) {
    Count i = 0;
    while (i < values.size()) {
        store.set(path / to_string(i), values[i]);
        i++;
    }
    while (store.count(path / to_string(i))) {
        store.erase(path / to_string(i));
        i++;
    }
}

void Set(const StorePath &path, const vector<Primitive> &data, const Count row_count, TransientStore &store) {
    assert(data.size() % row_count == 0);
    const Count col_count = data.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            store.set(path / to_string(row) / to_string(col), data[row * col_count + col]);
            col++;
        }
        while (store.count(path / to_string(row) / to_string(col))) {
            store.erase(path / to_string(row) / to_string(col));
            col++;
        }
        row++;
    }

    while (store.count(path / to_string(row) / to_string(0))) {
        Count col = 0;
        while (store.count(path / to_string(row) / to_string(col))) {
            store.erase(path / to_string(row) / to_string(col));
            col++;
        }
        row++;
    }
}

void Set(const Store &store) {
    ApplicationStore = store;
}
} // namespace store
