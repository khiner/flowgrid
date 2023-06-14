#include "Store.h"

#include "immer/algorithm.hpp"
#include <range/v3/core.hpp>

#include "Core/PrimitiveJson.h"
#include "StoreImpl.h"
#include "TransientStoreImpl.h"

using std::vector;

namespace store {
void ApplyPatch(const Patch &patch) {
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        if (op.Op == PatchOp::Type::Add || op.Op == PatchOp::Type::Replace) Set(path, *op.Value);
        else if (op.Op == PatchOp::Type::Remove) Erase(path);
    }
}

void ActionHandler::Apply(const Action::Store::Any &action) {
    Visit(
        action,
        [](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
    );
}

Store AppStore{};

nlohmann::json GetJson(const Store &store) {
    nlohmann::json j;
    for (const auto &[key, value] : store.PrimitiveForPath) {
        j[nlohmann::json::json_pointer(key.string())] = value;
    }
    return j;
}

const Store &Get() { return AppStore; }

nlohmann::json GetJson() { return GetJson(AppStore); }

Store JsonToStore(const nlohmann::json &j) {
    const auto &flattened = j.flatten();
    std::vector<std::pair<StorePath, Primitive>> entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value] : flattened.items()) entries[item_index++] = {StorePath(key), Primitive(value)};

    TransientStore::Map primitive_for_path;
    for (const auto &[path, value] : entries) primitive_for_path.set(path, value);
    return {primitive_for_path.persistent()};
}

TransientStore Transient{};
bool IsTransient = true;

void BeginTransient() {
    if (IsTransient) return;

    Transient = AppStore.Transient();
    IsTransient = true;
}

// End transient mode and return the new persistent store.
// Not exposed publicly (use `Commit` instead).
const Store EndTransient() {
    if (!IsTransient) return AppStore;

    const Store new_store = Transient.Persistent();
    Transient = {};
    IsTransient = false;

    return new_store;
}

void Commit() {
    AppStore = EndTransient();
}

Patch CheckedSet(const Store &store) {
    const auto &patch = CreatePatch(store);
    if (patch.Empty()) return {};

    AppStore = store;
    return patch;
}

Patch CheckedSetJson(const nlohmann::json &j) { return CheckedSet(JsonToStore(j)); }
Patch CheckedCommit() { return CheckedSet(EndTransient()); }

Store GetPersistent() { return Transient.Persistent(); }

Primitive Get(const StorePath &path) { return IsTransient ? Transient.PrimitiveForPath.at(path) : AppStore.PrimitiveForPath.at(path); }
void Set(const StorePath &path, const Primitive &value) {
    if (IsTransient) Transient.PrimitiveForPath.set(path, value);
    else auto _ = AppStore.PrimitiveForPath.set(path, value);
}
void Erase(const StorePath &path) {
    if (IsTransient) Transient.PrimitiveForPath.erase(path);
    else auto _ = AppStore.PrimitiveForPath.erase(path);
}

Count CountAt(const StorePath &path) { return IsTransient ? Transient.PrimitiveForPath.count(path) : AppStore.PrimitiveForPath.count(path); }

Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path) {
    PatchOps ops{};
    diff(
        before.PrimitiveForPath,
        after.PrimitiveForPath,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(base_path)] = {PatchOp::Type::Add, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(base_path)] = {PatchOp::Type::Replace, new_element.second, old_element.second};
        }
    );

    return {ops, base_path};
}

Patch CreatePatch(const Store &store, const StorePath &base_path) {
    return CreatePatch(AppStore, store, base_path);
}

Patch CreatePatch(const StorePath &base_path) {
    return CreatePatch(AppStore, EndTransient(), base_path);
}

void Set(const std::vector<std::pair<StorePath, Primitive>> &values) {
    for (const auto &[path, value] : values) Set(path, value);
}
void Set(const StorePath &path, const vector<Primitive> &values) {
    Count i = 0;
    while (i < values.size()) {
        Set(path / to_string(i), values[i]);
        i++;
    }
    while (CountAt(path / to_string(i))) {
        Erase(path / to_string(i));
        i++;
    }
}

void Set(const StorePath &path, const vector<Primitive> &data, const Count row_count) {
    assert(data.size() % row_count == 0);
    const Count col_count = data.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            Set(path / to_string(row) / to_string(col), data[row * col_count + col]);
            col++;
        }
        while (CountAt(path / to_string(row) / to_string(col))) {
            Erase(path / to_string(row) / to_string(col));
            col++;
        }
        row++;
    }

    while (CountAt(path / to_string(row) / to_string(0))) {
        Count col = 0;
        while (CountAt(path / to_string(row) / to_string(col))) {
            Erase(path / to_string(row) / to_string(col));
            col++;
        }
        row++;
    }
}
} // namespace store
