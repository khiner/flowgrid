#pragma once

#include "immer/map.hpp"

#include "Core/Action/Actionable.h"
#include "Core/Primitive/PrimitiveVariant.h"
#include "Helper/Path.h"
#include "IdPairs.h"
#include "Patch/Patch.h"
#include "StoreAction.h"

struct TransientStore;

struct Store : Actionable<Action::Store::Any> {
    using PrimitiveMap = immer::map<StorePath, PrimitiveVariant, PathHash>;
    using IdPairsMap = immer::map<StorePath, IdPairs, PathHash>;

    Store();
    Store(const Store &); // Copy constructor
    Store(Store &&);
    Store(PrimitiveMap &&, IdPairsMap &&);
    ~Store();

    void Set(Store &&);
    void Set(const Store &);

    TransientStore ToTransient() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    PrimitiveVariant Get(const StorePath &) const;
    u32 CountAt(const StorePath &) const;

    Store Get() const;

    void Set(const StorePath &, const PrimitiveVariant &) const;
    void Erase(const StorePath &) const;

    bool Exists(const StorePath &) const;

    IdPairs GetIdPairs(const StorePath &) const;
    bool HasIdPair(const StorePath &, const IdPair &) const;

    u32 IdPairCount(const StorePath &) const;
    void AddIdPair(const StorePath &, const IdPair &) const;
    void EraseIdPair(const StorePath &, const IdPair &) const;
    void ClearIdPairs(const StorePath &) const;

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const Store &);

    void Commit(); // Overwrite the persistent store with all changes since the last commit.
    Patch CheckedCommit(); // Same as `Commit`, but returns the resulting patch.

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath) const;
    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const Store &, const StorePath &base_path = RootPath) const;
    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath);

private:
    void ApplyPatch(const Patch &) const;

    PrimitiveMap PrimitiveByPath;
    IdPairsMap IdPairsByPath;
    std::unique_ptr<TransientStore> Transient; // If this is non-null, the store is in transient mode.
};
