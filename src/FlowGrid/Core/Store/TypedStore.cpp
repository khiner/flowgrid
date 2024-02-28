#include "Store.h"

#include "immer/algorithm.hpp"

using std::string;

// Naive `diff` method for `immer::vector`s.
// Callbacks receive an index and a value (for `add` and `remove`) or two values (for `change`).
template<typename T, typename Add, typename Remove, typename Change>
void diff(const immer::vector<T> &before, const immer::vector<T> &after, Add add, Remove remove, Change change) {
    std::size_t before_index = 0, after_index = 0;
    // Iterate over both vectors as long as both have elements remaining.
    while (before_index < before.size() && after_index < after.size()) {
        const auto &before_value = before[before_index];
        const auto &after_value = after[after_index];
        if (before_value != after_value) {
            change(before_index, before_value, after_value);
        }
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

template<typename ValueType, typename... ValueTypes>
void AddOps(const TypedStore<ValueTypes...> &before, const TypedStore<ValueTypes...> &after, const StorePath &base_path, PatchOps &ops) {
    diff(
        before.template GetMap<ValueType>(),
        after.template GetMap<ValueType>(),
        [&](const auto &added) { ops[added.first.lexically_relative(base_path)] = {PatchOpType::Add, added.second, {}}; },
        [&](const auto &removed) { ops[removed.first.lexically_relative(base_path)] = {PatchOpType::Remove, {}, removed.second}; },
        [&](const auto &o, const auto &n) { ops[o.first.lexically_relative(base_path)] = {PatchOpType::Replace, n.second, o.second}; }
    );
}

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatch(const TypedStore<ValueTypes...> &before, const TypedStore<ValueTypes...> &after, const StorePath &base_path) const {
    PatchOps ops{};

    AddOps<bool>(before, after, base_path, ops);
    AddOps<u32>(before, after, base_path, ops);
    AddOps<s32>(before, after, base_path, ops);
    AddOps<float>(before, after, base_path, ops);
    AddOps<string>(before, after, base_path, ops);

    diff(
        before.GetMap<IdPairs>(),
        after.GetMap<IdPairs>(),
        [&](const auto &added) {
            for (const auto &id_pair : added.second) {
                const auto serialized = SerializeIdPair(id_pair);
                ops[added.first.lexically_relative(base_path) / serialized] = {PatchOpType::Add, serialized, {}};
            }
        },
        [&](const auto &removed) {
            for (const auto &id_pair : removed.second) {
                const auto serialized = SerializeIdPair(id_pair);
                ops[removed.first.lexically_relative(base_path) / serialized] = {PatchOpType::Remove, {}, serialized};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](const auto &added) {
                    const auto serialized = SerializeIdPair(added);
                    ops[n.first.lexically_relative(base_path) / serialized] = {PatchOpType::Add, serialized, {}};
                },
                [&](const auto &removed) {
                    const auto serialized = SerializeIdPair(removed);
                    ops[o.first.lexically_relative(base_path) / serialized] = {PatchOpType::Remove, {}, serialized};
                },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
    diff(
        before.GetMap<immer::set<u32>>(),
        after.GetMap<immer::set<u32>>(),
        [&](const auto &added) {
            for (auto value : added.second) {
                ops[added.first.lexically_relative(base_path) / std::to_string(value)] = {PatchOpType::Add, value, {}};
            }
        },
        [&](const auto &removed) {
            for (auto value : removed.second) {
                ops[removed.first.lexically_relative(base_path) / std::to_string(value)] = {PatchOpType::Remove, {}, value};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                [&](auto added) { ops[n.first.lexically_relative(base_path) / std::to_string(added)] = {PatchOpType::Add, added, {}}; },
                [&](unsigned int removed) { ops[o.first.lexically_relative(base_path) / std::to_string(removed)] = {PatchOpType::Remove, {}, removed}; },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );
    diff(
        before.GetMap<immer::vector<u32>>(),
        after.GetMap<immer::vector<u32>>(),
        [&](const auto &added) {
            for (uint i = 0; i < added.second.size(); i++) {
                ops[added.first.lexically_relative(base_path) / std::to_string(i)] = {PatchOpType::Add, added.second[i], {}};
            }
        },
        [&](const auto &removed) {
            for (uint i = 0; i < removed.second.size(); i++) {
                ops[removed.first.lexically_relative(base_path) / std::to_string(i)] = {PatchOpType::Remove, {}, removed.second[i]};
            }
        },
        [&](const auto &o, const auto &n) {
            diff(
                o.second,
                n.second,
                // `diff` for `immer::vector<T>` provides callback values of type `pair<std::size_t, const T&>`,
                // where the first element is the index and the second is the value.
                [&](std::size_t i, u32 added) { ops[n.first.lexically_relative(base_path) / std::to_string(i)] = {PatchOpType::Add, added, {}}; },
                [&](std::size_t i, u32 removed) { ops[o.first.lexically_relative(base_path) / std::to_string(i)] = {PatchOpType::Remove, {}, removed}; },
                [&](std::size_t i, u32 o_el, u32 n_el) {
                    ops[o.first.lexically_relative(base_path) / std::to_string(i)] = {PatchOpType::Replace, n_el, o_el};
                }
            );
        }
    );

    return {ops, base_path};
}

// Utility to transform a tuple into another tuple, applying a function to each element.
template<typename ResultTuple, typename InputTuple, typename Func, std::size_t... I>
ResultTuple TransformTupleImpl(InputTuple &in, Func func, std::index_sequence<I...>) {
    return {func(std::get<I>(in))...};
}
template<typename ResultTuple, typename InputTuple, typename Func>
ResultTuple TransformTuple(InputTuple &in, Func func) {
    return TransformTupleImpl<ResultTuple>(in, func, std::make_index_sequence<std::tuple_size_v<InputTuple>>{});
}

template<typename... ValueTypes> TypedStore<ValueTypes...>::StoreMaps TypedStore<ValueTypes...>::Persistent() const {
    if (!TransientMaps) throw std::runtime_error("Store is not in transient mode.");
    return TransformTuple<StoreMaps>(*TransientMaps, [](auto &map) { return map.persistent(); });
}
template<typename... ValueTypes> TypedStore<ValueTypes...>::TransientStoreMaps TypedStore<ValueTypes...>::Transient() const {
    return TransformTuple<TransientStoreMaps>(Maps, [](auto &map) { return map.transient(); });
}

// Explicit instantiations for the default value types.
template struct TypedStore<bool, u32, s32, float, std::string, IdPairs, immer::set<u32>, immer::vector<u32>>;
