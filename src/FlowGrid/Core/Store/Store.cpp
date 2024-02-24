#include "Store.h"

#include "immer/algorithm.hpp"

#include "TransientStore.h"

// The store starts in transient mode.
Store::Store() : Transient(std::make_unique<TransientStore>()) {}
Store::Store(Store &&other) { Set(std::move(other)); }
Store::Store(const Store &other) { Set(other); }

Store::Store(PrimitiveMap &&primitives, IdPairsMap &&id_pairs)
    : PrimitiveByPath(std::move(primitives)), IdPairsByPath(std::move(id_pairs)), Transient(std::make_unique<TransientStore>(ToTransient())) {}

Store::~Store() = default;

TransientStore Store::ToTransient() const { return {PrimitiveByPath.transient(), IdPairsByPath.transient()}; }

void Store::ApplyPatch(const Patch &patch) const {
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        if (op.Op == PatchOp::Type::Add || op.Op == PatchOp::Type::Replace) Set(path, *op.Value);
        else if (op.Op == PatchOp::Type::Remove) Erase(path);
    }
}

void Store::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
        },
        action
    );
}

PrimitiveVariant Store::Get(const StorePath &path) const { return Transient->PrimitiveByPath.at(path); }
u32 Store::CountAt(const StorePath &path) const { return Transient->PrimitiveByPath.count(path); }

void Store::Set(const StorePath &path, const PrimitiveVariant &value) const { Transient->PrimitiveByPath.set(path, value); }
void Store::Erase(const StorePath &path) const { Transient->PrimitiveByPath.erase(path); }

// bool Store::HasPathStartingWith(const StorePath &path) const {
//     const auto keys = std::views::keys(PrimitiveByPath);
//     // return std::any_of(keys.begin(), keys.end(), [&path](const auto &key) { return key.starts_with(path); });

//     return std::ranges::any_of(keys, [&path](const StorePath &candidate_path) {
//         const auto &[first_mismatched_path_it, _] = std::mismatch(path.begin(), path.end(), candidate_path.begin(), candidate_path.end());
//         return first_mismatched_path_it == path.end();
//     });
// }

bool Store::Exists(const StorePath &path) const {
    // xxx this is the only place in the store where we use knowledge about vector paths.
    // It likely will soon _not_ be the only place, though, if we decide to use a `VectorsByPath`, though.
    return Transient->PrimitiveByPath.count(path) > 0 ||
        Transient->PrimitiveByPath.count(path / "0") > 0 ||
        Transient->IdPairsByPath.count(path) > 0;
}

IdPairs Store::GetIdPairs(const StorePath &path) const { return Transient->IdPairsByPath[path]; }
u32 Store::IdPairCount(const StorePath &path) const { return Transient->IdPairsByPath[path].size(); }
void Store::AddIdPair(const StorePath &path, const IdPair &value) const {
    if (!Transient->IdPairsByPath.count(path)) Transient->IdPairsByPath.set(path, {});
    Transient->IdPairsByPath.set(path, Transient->IdPairsByPath.at(path).insert(value));
}
void Store::EraseIdPair(const StorePath &path, const IdPair &value) const {
    if (!Transient->IdPairsByPath.count(path)) return;
    Transient->IdPairsByPath.set(path, Transient->IdPairsByPath.at(path).erase(value));
}
void Store::ClearIdPairs(const StorePath &path) const { Transient->IdPairsByPath.set(path, {}); }

bool Store::HasIdPair(const StorePath &path, const IdPair &value) const {
    if (!IdPairsByPath.count(path)) return false;
    return IdPairsByPath[path].count(value) > 0;
}

Store Store::Get() const { return Transient ? Transient->Persistent() : *this; }

void Store::Set(Store &&other) {
    PrimitiveByPath = other.PrimitiveByPath;
    IdPairsByPath = other.IdPairsByPath;
    Transient = std::make_unique<TransientStore>(other.ToTransient());
}
void Store::Set(const Store &other) {
    PrimitiveByPath = other.PrimitiveByPath;
    IdPairsByPath = other.IdPairsByPath;
    Transient = std::make_unique<TransientStore>(other.ToTransient());
}

void Store::Commit() { Set(Transient->Persistent()); }

Patch Store::CheckedSet(const Store &store) {
    const auto patch = CreatePatch(store);
    Set(store);
    return patch;
}

Patch Store::CheckedCommit() { return CheckedSet(Transient->Persistent()); }

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
                // Change callback required but never called for `immer::set`.
                [](const auto &, const auto &) {}
            );
        }
    );

    return {ops, base_path};
}

Patch Store::CreatePatch(const Store &store, const StorePath &base_path) const { return CreatePatch(*this, store, base_path); }

Patch Store::CreatePatchAndResetTransient(const StorePath &base_path) {
    const auto patch = CreatePatch(*this, Transient->Persistent(), base_path);
    Transient = std::make_unique<TransientStore>(ToTransient());
    return patch;
}
