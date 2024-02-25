#include "Store.h"

#include "immer/algorithm.hpp"

Patch Store::CreatePatch(const Store &before, const Store &after, const StorePath &base_path) const {
    PatchOps ops{};

    diff(
        before.PrimitiveByPath,
        after.PrimitiveByPath,
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
        before.IdPairsByPath,
        after.IdPairsByPath,
        [&](const auto &added) {
            for (const auto &id_pair : added.second) {
                ops[added.first.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(id_pair), {}};
            }
        },
        [&](const auto &removed) {
            for (const auto &id_pair : removed.second) {
                ops[removed.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(id_pair)};
            }
        },
        [&](const auto &old_element, const auto &new_element) {
            diff(
                old_element.second,
                new_element.second,
                [&](const auto &added) {
                    ops[new_element.first.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(added), {}};
                },
                [&](const auto &removed) {
                    ops[old_element.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(removed)};
                },
                [](const auto &, const auto &) {} // Change callback required but never called for `immer::set`.
            );
        }
    );

    return {ops, base_path};
}
