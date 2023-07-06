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
    Count CountAt(const StorePath &) const;

    void Set(const StorePath &, const Primitive &) const;
    void Erase(const StorePath &) const;

    Count IdPairCount(const StorePath &) const;
    std::unordered_set<IdPair, IdPairHash> IdPairs(const StorePath &) const;
    void AddIdPair(const StorePath &, const IdPair &) const;
    void EraseIdPair(const StorePath &, const IdPair &) const;
    bool HasIdPair(const StorePath &, const IdPair &) const;

    StoreImpl Get() const; // Get the current (concrete) store.
    nlohmann::json GetJson() const; // Get the current store as JSON.
    nlohmann::json GetJson(const StoreImpl &) const; // Get the provided store as JSON.

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const StoreImpl &);
    Patch CheckedSet(StoreImpl &&);
    Patch CheckedSetJson(nlohmann::json &&);

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
