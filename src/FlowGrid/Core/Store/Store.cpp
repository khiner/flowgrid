#include "Store.h"

#include <set>

#include "immer/algorithm.hpp"

#include "Core/Primitive/PrimitiveJson.h"
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

static const std::string IdPairsPrefix = "id_pairs::";

using namespace nlohmann;

json Store::GetJson(const StoreImpl &impl) const {
    // TODO serialize using the concrete primitive type and avoid the ambiguous Primitive JSON conversion.
    //   - This will be easier after separating container storage, since each `PrimitiveByPath` entry will correspond to a single `PrimitiveField`.
    json j;
    for (const auto &[path, primitive] : impl.PrimitiveByPath) {
        j[json::json_pointer(path.string())] = primitive;
    }
    for (const auto &[path, id_pairs] : impl.IdPairsByPath) {
        j[json::json_pointer(path.string())] = std::format("{}{}", IdPairsPrefix, json(id_pairs).dump());
    }
    return j;
}

static StoreImpl JsonToStore(json &&j) {
    const auto &flattened = j.flatten();
    TransientStoreImpl transient;
    for (const auto &[key, value] : flattened.items()) {
        const StorePath path = key;
        if (value.is_string() && std::string(value).starts_with(IdPairsPrefix)) {
            const auto &id_pairs = json::parse(std::string(value).substr(IdPairsPrefix.size()));
            for (const auto &id_pair : id_pairs) transient.IdPairsByPath[path].insert(id_pair);
        } else {
            transient.PrimitiveByPath.set(path, std::move(value));
        }
    }

    return transient.Persistent();
}

StoreImpl Store::Get() const { return TransientImpl ? TransientImpl->Persistent() : *Impl; }
json Store::GetJson() const { return GetJson(*Impl); }

void Store::BeginTransient() {
    if (TransientImpl) return;

    TransientImpl = std::make_unique<TransientStoreImpl>(Impl->Transient());
}

// End transient mode and return the new persistent store.
// Not exposed publicly (use `Commit` instead).
StoreImpl Store::EndTransient() {
    if (!TransientImpl) return *Impl;

    const StoreImpl new_store = TransientImpl->Persistent();
    TransientImpl.reset();

    return new_store;
}

void Store::Commit() {
    Impl = std::make_unique<StoreImpl>(EndTransient());
}

Patch Store::CheckedSet(const StoreImpl &store) {
    TransientImpl.reset();
    const auto &patch = CreatePatch(store);
    if (patch.Empty()) return {};

    Impl = std::make_unique<StoreImpl>(store);
    return patch;
}

Patch Store::CheckedSet(StoreImpl &&store) {
    TransientImpl.reset();
    const auto &patch = CreatePatch(store);
    if (patch.Empty()) return {};

    Impl = std::make_unique<StoreImpl>(std::move(store));
    return patch;
}

Patch Store::CheckedCommit() { return CheckedSet(EndTransient()); }
Patch Store::SetJson(json &&j) { return CheckedSet(JsonToStore(std::move(j))); }

Primitive Store::Get(const StorePath &path) const { return TransientImpl ? TransientImpl->PrimitiveByPath.at(path) : Impl->PrimitiveByPath.at(path); }

void Store::Set(const StorePath &path, const Primitive &value) const {
    if (TransientImpl) {
        TransientImpl->PrimitiveByPath.set(path, value);
    } else {
        // todo no effect. throw error instead?
        auto _ = Impl->PrimitiveByPath.set(path, value);
    }
}
void Store::Erase(const StorePath &path) const {
    if (TransientImpl) {
        TransientImpl->PrimitiveByPath.erase(path);
    } else {
        auto _ = Impl->PrimitiveByPath.erase(path);
    }
}

Count Store::CheckedCommit(const StorePath &path) const {
    return TransientImpl ? TransientImpl->IdPairsByPath[path].size() : Impl->IdPairsByPath[path].size();
}

Count Store::IdPairCount(const StorePath &path) const {
    return TransientImpl ? TransientImpl->IdPairsByPath[path].size() : Impl->IdPairsByPath[path].size();
}

std::unordered_set<IdPair, IdPairHash> Store::IdPairs(const StorePath &path) const {
    std::unordered_set<IdPair, IdPairHash> id_pairs;
    if (TransientImpl) {
        for (const auto &id_pair : TransientImpl->IdPairsByPath[path]) id_pairs.insert(id_pair);
    } else {
        for (const auto &id_pair : Impl->IdPairsByPath[path]) id_pairs.insert(id_pair);
    }
    return id_pairs;
}

void Store::AddIdPair(const StorePath &path, const IdPair &value) const {
    if (TransientImpl) {
        TransientImpl->IdPairsByPath[path].insert(value);
    } else {
        auto _ = Impl->IdPairsByPath[path].insert(value);
    }
}
void Store::EraseIdPair(const StorePath &path, const IdPair &value) const {
    if (TransientImpl) {
        TransientImpl->IdPairsByPath[path].erase(value);
    } else {
        auto _ = Impl->IdPairsByPath[path].erase(value);
    }
}
bool Store::HasIdPair(const StorePath &path, const IdPair &value) const {
    if (TransientImpl) {
        if (!TransientImpl->IdPairsByPath.contains(path)) return false;
        return TransientImpl->IdPairsByPath[path].count(value) > 0;
    } else {
        if (!Impl->IdPairsByPath.contains(path)) return false;
        return Impl->IdPairsByPath[path].count(value) > 0;
    }
}

Count Store::CountAt(const StorePath &path) const { return TransientImpl ? TransientImpl->PrimitiveByPath.count(path) : Impl->PrimitiveByPath.count(path); }

Patch Store::CreatePatch(const StoreImpl &before, const StoreImpl &after, const StorePath &base_path) const {
    PatchOps ops{};

    diff(
        before.PrimitiveByPath,
        after.PrimitiveByPath,
        [&](auto const &added) {
            ops[added.first.lexically_relative(base_path)] = {PatchOp::Type::Add, added.second, {}};
        },
        [&](auto const &removed) {
            ops[removed.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, removed.second};
        },
        [&](auto const &old_element, auto const &new_element) {
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
Patch Store::CreatePatch(const StorePath &base_path) { return CreatePatch(*Impl, EndTransient(), base_path); }
