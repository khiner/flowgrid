#include "Store.h"

#include "immer/algorithm.hpp"

using std::string;

// Utility to transform a tuple into another tuple, applying a function to each element.
template<typename ResultTuple, typename InputTuple, typename Func, std::size_t... I>
ResultTuple TransformTupleImpl(InputTuple &in, Func func, std::index_sequence<I...>) {
    return {func(std::get<I>(in))...};
}
template<typename ResultTuple, typename InputTuple, typename Func>
ResultTuple TransformTuple(InputTuple &in, Func func) {
    return TransformTupleImpl<ResultTuple>(in, func, std::make_index_sequence<std::tuple_size_v<InputTuple>>{});
}

Store::StoreMaps Store::Persistent() const {
    if (!TransientMaps) throw std::runtime_error("Store is not in transient mode.");
    return TransformTuple<StoreMaps>(*TransientMaps, [](auto &map) { return map.persistent(); });
}
Store::TransientStoreMaps Store::Transient() const {
    return TransformTuple<TransientStoreMaps>(Maps, [](auto &map) { return map.transient(); });
}

Patch Store::CreatePatch(const Store &before, const Store &after, const StorePath &base_path) const {
    PatchOps ops{};

    diff(
        before.GetMap<PrimitiveVariant>(),
        after.GetMap<PrimitiveVariant>(),
        [&](const auto &added) {
            ops[added.first.lexically_relative(base_path)] = {PatchOp::Type::Add, added.second, {}};
        },
        [&](const auto &removed) {
            ops[removed.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, removed.second};
        },
        [&](const auto &old_element, const auto &new_element) {
            ops[old_element.first.lexically_relative(base_path)] = {PatchOp::Type::Replace, new_element.second, old_element.second};
        }
    );

    diff(
        before.GetMap<IdPairs>(),
        after.GetMap<IdPairs>(),
        [&](const auto &added) {
            for (const auto &id_pair : added.second) {
                auto serialized = SerializeIdPair(id_pair);
                ops[added.first.lexically_relative(base_path) / serialized] = {PatchOp::Type::Add, serialized, {}};
            }
        },
        [&](const auto &removed) {
            for (const auto &id_pair : removed.second) {
                auto serialized = SerializeIdPair(id_pair);
                ops[removed.first.lexically_relative(base_path) / serialized] = {PatchOp::Type::Remove, {}, serialized};
            }
        },
        [&](const auto &old_element, const auto &new_element) {
            diff(
                old_element.second,
                new_element.second,
                [&](const auto &added) {
                    auto serialized = SerializeIdPair(added);
                    ops[new_element.first.lexically_relative(base_path) / serialized] = {PatchOp::Type::Add, serialized, {}};
                },
                [&](const auto &removed) {
                    auto serialized = SerializeIdPair(removed);
                    ops[old_element.first.lexically_relative(base_path) / serialized] = {PatchOp::Type::Remove, {}, serialized};
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
                ops[added.first.lexically_relative(base_path) / std::to_string(value)] = {PatchOp::Type::Add, value, {}};
            }
        },
        [&](const auto &removed) {
            for (auto value : removed.second) {
                ops[removed.first.lexically_relative(base_path) / std::to_string(value)] = {PatchOp::Type::Remove, {}, value};
            }
        },
        [&](const auto &old_element, const auto &new_element) {
            diff(
                old_element.second,
                new_element.second,
                [&](auto added) {
                    ops[new_element.first.lexically_relative(base_path) / std::to_string(added)] = {PatchOp::Type::Add, added, {}};
                },
                [&](unsigned int removed) {
                    ops[old_element.first.lexically_relative(base_path) / std::to_string(removed)] = {PatchOp::Type::Remove, {}, removed};
                },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );

    return {ops, base_path};
}
