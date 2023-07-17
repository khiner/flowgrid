#include "Store.h"

#include "immer/algorithm.hpp"

#include "StoreImpl.h"
#include "TransientStoreImpl.h"

// The store starts in transient mode.
Store::Store() : Impl(std::make_unique<StoreImpl>()), TransientImpl(std::make_unique<TransientStoreImpl>()) {}
Store::~Store() = default;

void Store::ApplyPatch(const Patch &patch) const {
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        if (op.Op == PatchOp::Type::Add || op.Op == PatchOp::Type::Replace) Set(path, *op.Value);
        else if (op.Op == PatchOp::Type::Remove) Erase(path);
    }
}

void Store::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
    );
}

Primitive Store::Get(const StorePath &path) const { return TransientImpl->PrimitiveByPath.at(path); }
u32 Store::CountAt(const StorePath &path) const { return TransientImpl->PrimitiveByPath.count(path); }

void Store::Set(const StorePath &path, const Primitive &value) const { TransientImpl->PrimitiveByPath.set(path, value); }
void Store::Erase(const StorePath &path) const { TransientImpl->PrimitiveByPath.erase(path); }

IdPairs Store::IdPairs(const StorePath &path) const {
    ::IdPairs id_pairs;
    for (const auto &id_pair : TransientImpl->IdPairsByPath[path]) id_pairs.insert(id_pair);
    return id_pairs;
}

u32 Store::IdPairCount(const StorePath &path) const { return TransientImpl->IdPairsByPath[path].size(); }

void Store::AddIdPair(const StorePath &path, const IdPair &value) const { TransientImpl->IdPairsByPath[path].insert(value); }
void Store::EraseIdPair(const StorePath &path, const IdPair &value) const { TransientImpl->IdPairsByPath[path].erase(value); }

void Store::ClearIdPairs(const StorePath &path) const {
    TransientImpl->IdPairsByPath[path] = {};
}

bool Store::HasIdPair(const StorePath &path, const IdPair &value) const {
    if (!TransientImpl->IdPairsByPath.contains(path)) return false;
    return TransientImpl->IdPairsByPath[path].count(value) > 0;
}

StoreImpl Store::Get() const { return TransientImpl ? TransientImpl->Persistent() : *Impl; }

void Store::Set(const StoreImpl &impl) {
    Impl = std::make_unique<StoreImpl>(std::move(impl));
    TransientImpl = std::make_unique<TransientStoreImpl>(Impl->Transient());
}
void Store::Set(StoreImpl &&impl) {
    Impl = std::make_unique<StoreImpl>(std::move(impl));
    TransientImpl = std::make_unique<TransientStoreImpl>(Impl->Transient());
}

void Store::Commit() { Set(TransientImpl->Persistent()); }

Patch Store::CheckedSet(const StoreImpl &store) {
    const auto patch = CreatePatch(store);
    Set(store);
    return patch;
}

Patch Store::CheckedCommit() { return CheckedSet(TransientImpl->Persistent()); }

Patch Store::CreatePatch(const StoreImpl &before, const StoreImpl &after, const StorePath &base_path) const {
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

    // Added IdPair sets:
    for (const auto &[id_pairs_path, id_pairs] : after.IdPairsByPath) {
        if (!before.IdPairsByPath.contains(id_pairs_path)) {
            for (const auto &id_pair : id_pairs) {
                ops[id_pairs_path.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(id_pair), {}};
            }
        }
    }
    // Removed IdPair sets:
    for (const auto &[id_pairs_path, id_pairs] : before.IdPairsByPath) {
        if (!after.IdPairsByPath.contains(id_pairs_path)) {
            for (const auto &id_pair : id_pairs) {
                ops[id_pairs_path.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(id_pair)};
            }
        }
    }
    // Changed IdPair sets:
    for (const auto &id_pair_path : std::views::keys(before.IdPairsByPath)) {
        if (!after.IdPairsByPath.contains(id_pair_path)) continue;
        diff(
            before.IdPairsByPath.at(id_pair_path),
            after.IdPairsByPath.at(id_pair_path),
            [&ops, &id_pair_path, &base_path](const auto &added) {
                ops[id_pair_path.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(added), {}};
            },
            [&ops, &id_pair_path, &base_path](const auto &removed) {
                ops[id_pair_path.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(removed)};
            },
            // Change callback required but never called for `immer::set`.
            [](const auto &, const auto &) {}
        );
    }

    return {ops, base_path};
}

Patch Store::CreatePatch(const StoreImpl &store, const StorePath &base_path) const { return CreatePatch(*Impl, store, base_path); }

Patch Store::CreatePatchAndResetTransient(const StorePath &base_path) {
    const auto patch = CreatePatch(*Impl, TransientImpl->Persistent(), base_path);
    TransientImpl = std::make_unique<TransientStoreImpl>(Impl->Transient());
    return patch;
}
