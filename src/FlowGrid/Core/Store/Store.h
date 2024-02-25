#pragma once

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include "immer/vector.hpp"

#include "Core/Action/Actionable.h"
#include "Core/Primitive/PrimitiveVariant.h"
#include "Helper/Path.h"
#include "IdPairs.h"
#include "Patch/Patch.h"
#include "StoreAction.h"

struct TransientStore {
    template<typename T> using Map = immer::map_transient<StorePath, T, PathHash>;

    Map<PrimitiveVariant> PrimitiveByPath;
    Map<IdPairs> IdPairsByPath;
};

struct Store : Actionable<Action::Store::Any> {
    template<typename T> using Map = immer::map<StorePath, T, PathHash>;

    // The store starts in transient mode.
    Store() : Transient(std::make_unique<TransientStore>()) {}
    Store(const Store &other) { Set(other); }
    Store(Store &&other) { Set(std::move(other)); }
    Store(Map<PrimitiveVariant> &&primitives, Map<IdPairs> &&id_pairs)
        : PrimitiveByPath(std::move(primitives)), IdPairsByPath(std::move(id_pairs)),
          Transient(std::make_unique<TransientStore>(ToTransient())) {}

    ~Store() = default;

    void Set(Store &&other) {
        PrimitiveByPath = other.PrimitiveByPath;
        IdPairsByPath = other.IdPairsByPath;
        Transient = std::make_unique<TransientStore>(other.ToTransient());
    }
    void Set(const Store &other) {
        PrimitiveByPath = other.PrimitiveByPath;
        IdPairsByPath = other.IdPairsByPath;
        Transient = std::make_unique<TransientStore>(other.ToTransient());
    }

    Store Persistent() const { return {Transient->PrimitiveByPath.persistent(), Transient->IdPairsByPath.persistent()}; }
    TransientStore ToTransient() const { return {PrimitiveByPath.transient(), IdPairsByPath.transient()}; }

    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
            },
            action
        );
    }

    bool CanApply(const ActionType &) const override { return true; }

    Store Get() const { return Transient ? Persistent() : *this; }

    PrimitiveVariant Get(const StorePath &path) const { return Transient->PrimitiveByPath.at(path); }
    u32 CountAt(const StorePath &path) const { return Transient->PrimitiveByPath.count(path); }

    void Set(const StorePath &path, const PrimitiveVariant &value) const { Transient->PrimitiveByPath.set(path, value); }
    void Erase(const StorePath &path) const { Transient->PrimitiveByPath.erase(path); }

    bool Exists(const StorePath &path) const {
        // xxx this is the only place in the store where we use knowledge about vector paths.
        // It likely will soon _not_ be the only place, though, if we decide to use a `VectorsByPath`, though.
        return Transient->PrimitiveByPath.count(path) > 0 ||
            Transient->PrimitiveByPath.count(path / "0") > 0 ||
            Transient->IdPairsByPath.count(path) > 0;
    }

    bool HasIdPair(const StorePath &path, const IdPair &value) const {
        if (!IdPairsByPath.count(path)) return false;
        return IdPairsByPath[path].count(value) > 0;
    }
    IdPairs GetIdPairs(const StorePath &path) const { return Transient->IdPairsByPath[path]; }
    u32 IdPairCount(const StorePath &path) const { return Transient->IdPairsByPath[path].size(); }
    void AddIdPair(const StorePath &path, const IdPair &value) const {
        if (!Transient->IdPairsByPath.count(path)) Transient->IdPairsByPath.set(path, {});
        Transient->IdPairsByPath.set(path, Transient->IdPairsByPath.at(path).insert(value));
    }
    void EraseIdPair(const StorePath &path, const IdPair &value) const {
        if (!Transient->IdPairsByPath.count(path)) return;
        Transient->IdPairsByPath.set(path, Transient->IdPairsByPath.at(path).erase(value));
    }
    void ClearIdPairs(const StorePath &path) const { Transient->IdPairsByPath.set(path, {}); }

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const Store &store) {
        const auto patch = CreatePatch(store);
        Set(store);
        return patch;
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Set(Persistent()); }
    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit() { return CheckedSet(Persistent()); }

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath) const;

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const Store &store, const StorePath &base_path = RootPath) const {
        return CreatePatch(*this, store, base_path);
    }
    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) {
        const auto patch = CreatePatch(*this, Persistent(), base_path);
        Transient = std::make_unique<TransientStore>(ToTransient());
        return patch;
    }

private:
    void ApplyPatch(const Patch &patch) const {
        for (const auto &[partial_path, op] : patch.Ops) {
            const auto path = patch.BasePath / partial_path;
            if (op.Op == PatchOp::Type::Add || op.Op == PatchOp::Type::Replace) Set(path, *op.Value);
            else if (op.Op == PatchOp::Type::Remove) Erase(path);
        }
    }

    Map<PrimitiveVariant> PrimitiveByPath;
    Map<IdPairs> IdPairsByPath;
    std::unique_ptr<TransientStore> Transient; // If this is non-null, the store is in transient mode.
};
