#include "TypedStore.h"

#include <ranges>

#include "immer/algorithm.hpp"

#include "IdPairs.h"
#include "Project/TextEditor/TextBufferData.h"

// `AddOps` function definitions for all specialized `ValueTypes`, to fully implement the `CreatePatch` method.

// Naive `diff` method for `immer::vector`s.
// Callbacks receive an index and a value (for `add` and `remove`) or two values (for `change`).
template<typename T, typename Add, typename Remove, typename Change>
void diff(const immer::flex_vector<T> &before, const immer::flex_vector<T> &after, Add add, Remove remove, Change change) {
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

// todo
// This is the only diff that assumes it's comparing _consecutive_ entries in history.
// It takes advantage of the fact that `TextBufferData` contains `Edits` representing the changes between each state.
// If we wanted to compare two arbitrary `TextBufferData` instances, we'd either need generic diffing for `immer::flex_vector`,
// or we'd need access to each `TextBufferData` between the two instances to accumulate the edits.
void AddOps(const StoreMap<TextBufferData> &before, const StoreMap<TextBufferData> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&ops](const auto &added) { ops[added.first].emplace_back(PatchOpType::Add, "", std::nullopt); },
        [&ops](const auto &removed) { ops[removed.first].emplace_back(PatchOpType::Remove, std::nullopt, ""); },
        [&ops](const auto &o, const auto &) { ops[o.first].emplace_back(PatchOpType::Replace, "", ""); }
    );
}

template<typename ValueType>
void AddOps(const StoreMap<ValueType> &before, const StoreMap<ValueType> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&ops](const auto &added) { ops[added.first].emplace_back(PatchOpType::Add, added.second, std::nullopt); },
        [&ops](const auto &removed) { ops[removed.first].emplace_back(PatchOpType::Remove, std::nullopt, removed.second); },
        [&ops](const auto &o, const auto &n) { ops[o.first].emplace_back(PatchOpType::Replace, n.second, o.second); }
    );
}

void AddOps(const StoreMap<IdPairs> &before, const StoreMap<IdPairs> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&ops](const auto &added) {
            for (const auto &id_pair : added.second) ops[added.first].emplace_back(PatchOpType::Insert, SerializeIdPair(id_pair), std::nullopt);
        },
        [&ops](const auto &removed) {
            for (const auto &id_pair : removed.second) ops[removed.first].emplace_back(PatchOpType::Erase, std::nullopt, SerializeIdPair(id_pair));
        },
        [&ops](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&ops, &n](const auto &added) { ops[n.first].emplace_back(PatchOpType::Insert, SerializeIdPair(added), std::nullopt); },
                [&ops, &o](const auto &removed) { ops[o.first].emplace_back(PatchOpType::Erase, std::nullopt, SerializeIdPair(removed)); },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

void AddOps(const StoreMap<immer::set<u32>> &before, const StoreMap<immer::set<u32>> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&ops](const auto &added) {
            for (auto v : added.second) ops[added.first].emplace_back(PatchOpType::Insert, v, std::nullopt);
        },
        [&ops](const auto &removed) {
            for (auto v : removed.second) ops[removed.first].emplace_back(PatchOpType::Erase, std::nullopt, v);
        },
        [&ops](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&ops, &n](auto added) { ops[n.first].emplace_back(PatchOpType::Insert, added, std::nullopt); },
                [&ops, &o](auto removed) { ops[o.first].emplace_back(PatchOpType::Erase, std::nullopt, removed); },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
}

template<typename T> void AddOps(const StoreMap<immer::flex_vector<T>> &before, const StoreMap<immer::flex_vector<T>> &after, PatchOps &ops) {
    diff(
        before,
        after,
        [&ops](const auto &added) {
            for (auto v : added.second) ops[added.first].emplace_back(PatchOpType::PushBack, v, std::nullopt);
        },
        [&ops](const auto &removed) {
            for (auto v : std::ranges::reverse_view(removed.second)) ops[removed.first].emplace_back(PatchOpType::PopBack, std::nullopt, v);
        },
        [&ops](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                // `diff` for `immer::vector<T>` provides callback values of type `pair<size_t, const T&>`,
                // where the first element is the index and the second is the value.
                [&ops, &n](size_t, T added) { ops[n.first].emplace_back(PatchOpType::PushBack, added, std::nullopt); },
                [&ops, &o](size_t, T removed) { ops[o.first].emplace_back(PatchOpType::PopBack, std::nullopt, removed); },
                // `PatchOpType::Set` op type is used to distinguish between primitive value changes and vector element changes.
                // (Primitive value changes are of type `PatchOpType::Replace`.)
                // This is also the only patch op path that does _not_ point straight to the component.
                [&ops, &o](size_t i, T o_el, T n_el) { ops[o.first].emplace_back(PatchOpType::Set, n_el, o_el, i); }
            );
        }
    );
}

template<typename ValueType>
void AddOps(const auto &before_maps, const auto &after_maps, PatchOps &ops) {
    AddOps(std::get<StoreMap<ValueType>>(before_maps), std::get<StoreMap<ValueType>>(after_maps), ops);
}

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatch(const TypedStore<ValueTypes...>::StoreMaps &before_maps, const TypedStore<ValueTypes...>::StoreMaps &after_maps, ID base_component_id) {
    PatchOps ops{};
    // Use template lambda to call `AddOps` for each value type.
    ([&]<typename T>() { AddOps<T>(before_maps, after_maps, ops); }.template operator()<ValueTypes>(), ...);
    return {base_component_id, ops};
}

// Explicit instantiation for the default value types.
template struct TypedStore<
    bool, u32, s32, float, std::string, IdPairs, TextBufferData, immer::set<u32>,
    immer::flex_vector<bool>, immer::flex_vector<s32>, immer::flex_vector<u32>, immer::flex_vector<float>, immer::flex_vector<std::string>>;
