#pragma once

#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "IdPair.h"
#include "Patch/Patch.h"
#include "StoreAction.h"

struct StoreImpl;
struct TransientStoreImpl;

struct Store : Actionable<Action::Store::Any> {
    Store();
    ~Store();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    Primitive Get(const StorePath &) const;
    u32 CountAt(const StorePath &) const;

    void Set(const StorePath &, const Primitive &) const;
    void Erase(const StorePath &) const;

    bool Exists(const StorePath &) const;

    IdPairs IdPairs(const StorePath &) const;
    bool HasIdPair(const StorePath &, const IdPair &) const;

    u32 IdPairCount(const StorePath &) const;
    void AddIdPair(const StorePath &, const IdPair &) const;
    void EraseIdPair(const StorePath &, const IdPair &) const;
    void ClearIdPairs(const StorePath &) const;

    StoreImpl Get() const; // Get the current (concrete) store.

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const StoreImpl &);

    void Commit(); // Overwrite the persistent store with all changes since the last commit.
    Patch CheckedCommit(); // Same as `Commit`, but returns the resulting patch.

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const StoreImpl &before, const StoreImpl &after, const StorePath &base_path = RootPath) const;
    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const StoreImpl &, const StorePath &base_path = RootPath) const;
    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath);

private:
    void ApplyPatch(const Patch &) const;

    void Set(const StoreImpl &);
    void Set(StoreImpl &&);

    std::unique_ptr<StoreImpl> Impl;
    std::unique_ptr<TransientStoreImpl> TransientImpl; // If this is non-null, the store is in transient mode.
};
