#include "Store.h"

#include <ranges>

#include "immer/algorithm.hpp"

// Naive `diff` method for `immer::vector`s.
// Callbacks receive an index and a value (for `add` and `remove`) or two values (for `change`).
template<typename T, typename Add, typename Remove, typename Change>
void diff(const immer::vector<T> &before, const immer::vector<T> &after, Add add, Remove remove, Change change) {
    size_t before_index = 0, after_index = 0;
    // Iterate over both vectors as long as both have elements remaining.
    while (before_index < before.size() && after_index < after.size()) {
        const auto &before_value = before[before_index];
        const auto &after_value = after[after_index];
        if (before_value != after_value) change(before_index, before_value, after_value);
        before_index++;
        after_index++;
    }
    // Process any remaining elements in the `before` vector as removed (pop_back).
    if (before_index < before.size()) {
        for (auto i = before.size(); i-- > before_index;) remove(i, before[i]);
    }
    // Process any remaining elements in the `after` vector as added (push_back).
    for (auto i = after_index; i < after.size(); ++i) add(i, after[i]);
}

template<typename ValueType>
void AddOps(const StoreMap<ValueType> &before, const StoreMap<ValueType> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) { ops[added.first].emplace_back(PatchOpType::Add, added.second, std::nullopt); },
        [&](const auto &removed) { ops[removed.first].emplace_back(PatchOpType::Remove, std::nullopt, removed.second); },
        [&](const auto &o, const auto &n) { ops[o.first].emplace_back(PatchOpType::Replace, n.second, o.second); }
    );
}

void AddOps(const StoreMap<IdPairs> &before, const StoreMap<IdPairs> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (const auto &id_pair : added.second) ops[added.first].emplace_back(PatchOpType::Insert, SerializeIdPair(id_pair), std::nullopt);
        },
        [&](const auto &removed) {
            for (const auto &id_pair : removed.second) ops[removed.first].emplace_back(PatchOpType::Erase, std::nullopt, SerializeIdPair(id_pair));
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](const auto &added) { ops[n.first].emplace_back(PatchOpType::Insert, SerializeIdPair(added), std::nullopt); },
                [&](const auto &removed) { ops[o.first].emplace_back(PatchOpType::Erase, std::nullopt, SerializeIdPair(removed)); },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

void AddOps(const StoreMap<immer::set<u32>> &before, const StoreMap<immer::set<u32>> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (auto v : added.second) ops[added.first].emplace_back(PatchOpType::Insert, v, std::nullopt);
        },
        [&](const auto &removed) {
            for (auto v : removed.second) ops[removed.first].emplace_back(PatchOpType::Erase, std::nullopt, v);
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](auto added) { ops[n.first].emplace_back(PatchOpType::Insert, added, std::nullopt); },
                [&](auto removed) { ops[o.first].emplace_back(PatchOpType::Erase, std::nullopt, removed); },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

template<typename T> void AddOps(const StoreMap<immer::vector<T>> &before, const StoreMap<immer::vector<T>> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&](const auto &added) {
            for (auto v : added.second) ops[added.first].emplace_back(PatchOpType::PushBack, v, std::nullopt);
        },
        [&](const auto &removed) {
            for (auto v : std::ranges::reverse_view(removed.second)) ops[removed.first].emplace_back(PatchOpType::PopBack, std::nullopt, v);
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                // `diff` for `immer::vector<T>` provides callback values of type `pair<size_t, const T&>`,
                // where the first element is the index and the second is the value.
                [&](size_t, T added) { ops[n.first].emplace_back(PatchOpType::PushBack, added, std::nullopt); },
                [&](size_t, T removed) { ops[o.first].emplace_back(PatchOpType::PopBack, std::nullopt, removed); },
                // `PatchOpType::Set` op type is used to distinguish between primitive value changes and vector element changes.
                // (Primitive value changes are of type `PatchOpType::Replace`.)
                // This is also the only patch op path that does _not_ point straight to the component.
                [&](size_t i, T o_el, T n_el) { ops[o.first].emplace_back(PatchOpType::Set, n_el, o_el, i); }
            );
        }
    );
}

// Explicit instantiation for the default value types.
template struct TypedStore<
    bool, u32, s32, float, std::string, IdPairs, immer::set<u32>,
    immer::vector<bool>, immer::vector<s32>, immer::vector<u32>, immer::vector<float>, immer::vector<std::string>>;
