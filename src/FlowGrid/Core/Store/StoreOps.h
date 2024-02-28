#include "Store.h"

#include "immer/algorithm.hpp"

using std::to_string;

// Naive `diff` method for `immer::vector`s.
// Callbacks receive an index and a value (for `add` and `remove`) or two values (for `change`).
template<typename T, typename Add, typename Remove, typename Change>
void diff(const immer::vector<T> &before, const immer::vector<T> &after, Add add, Remove remove, Change change) {
    std::size_t before_index = 0, after_index = 0;
    // Iterate over both vectors as long as both have elements remaining.
    while (before_index < before.size() && after_index < after.size()) {
        const auto &before_value = before[before_index];
        const auto &after_value = after[after_index];
        if (before_value != after_value) change(before_index, before_value, after_value);
        before_index++;
        after_index++;
    }
    // Process any remaining elements in the `before` vector as removed.
    while (before_index < before.size()) {
        remove(before_index, before[before_index]);
        before_index++;
    }
    // Process any remaining elements in the `after` vector as added.
    while (after_index < after.size()) {
        add(after_index, after[after_index]);
        after_index++;
    }
}

static auto Path(const auto &entry, const StorePath &base) { return entry.first.lexically_relative(base); }

template<typename ValueType>
void AddOps(const StoreMap<ValueType> &before, const StoreMap<ValueType> &after, const StorePath &base, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) { ops[Path(added, base)] = {PatchOpType::Add, added.second, {}}; },
        [&](const auto &removed) { ops[Path(removed, base)] = {PatchOpType::Remove, {}, removed.second}; },
        [&](const auto &o, const auto &n) { ops[Path(o, base)] = {PatchOpType::Replace, n.second, o.second}; }
    );
}

void AddOps(const StoreMap<IdPairs> &before, const StoreMap<IdPairs> &after, const StorePath &base, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (const auto &id_pair : added.second) {
                const auto serialized = SerializeIdPair(id_pair);
                ops[Path(added, base) / serialized] = {PatchOpType::Add, serialized, {}};
            }
        },
        [&](const auto &removed) {
            for (const auto &id_pair : removed.second) {
                const auto serialized = SerializeIdPair(id_pair);
                ops[Path(removed, base) / serialized] = {PatchOpType::Remove, {}, serialized};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](const auto &added) {
                    const auto serialized = SerializeIdPair(added);
                    ops[Path(n, base) / serialized] = {PatchOpType::Add, serialized, {}};
                },
                [&](const auto &removed) {
                    const auto serialized = SerializeIdPair(removed);
                    ops[Path(o, base) / serialized] = {PatchOpType::Remove, {}, serialized};
                },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

void AddOps(const StoreMap<immer::set<u32>> &before, const StoreMap<immer::set<u32>> &after, const StorePath &base, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (auto value : added.second) {
                ops[Path(added, base) / to_string(value)] = {PatchOpType::Add, value, {}};
            }
        },
        [&](const auto &removed) {
            for (auto value : removed.second) {
                ops[Path(removed, base) / to_string(value)] = {PatchOpType::Remove, {}, value};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](auto added) { ops[Path(n, base) / to_string(added)] = {PatchOpType::Add, added, {}}; },
                [&](unsigned int removed) { ops[Path(o, base) / to_string(removed)] = {PatchOpType::Remove, {}, removed}; },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

void AddOps(const StoreMap<immer::vector<u32>> &before, const StoreMap<immer::vector<u32>> &after, const StorePath &base, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (size_t i = 0; i < added.second.size(); ++i) {
                ops[Path(added, base) / to_string(i)] = {PatchOpType::Add, added.second[i], {}};
            }
        },
        [&](const auto &removed) {
            for (size_t i = 0; i < removed.second.size(); ++i) {
                ops[Path(removed, base) / to_string(i)] = {PatchOpType::Remove, {}, removed.second[i]};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                // `diff` for `immer::vector<T>` provides callback values of type `pair<std::size_t, const T&>`,
                // where the first element is the index and the second is the value.
                [&](size_t i, u32 added) { ops[Path(n, base) / to_string(i)] = {PatchOpType::Add, added, {}}; },
                [&](size_t i, u32 removed) { ops[Path(o, base) / to_string(i)] = {PatchOpType::Remove, {}, removed}; },
                [&](size_t i, u32 o_el, u32 n_el) { ops[Path(o, base) / to_string(i)] = {PatchOpType::Replace, n_el, o_el}; }
            );
        }
    );
}

// Explicit instantiation for the default value types.
template struct TypedStore<bool, u32, s32, float, std::string, IdPairs, immer::set<u32>, immer::vector<u32>>;
