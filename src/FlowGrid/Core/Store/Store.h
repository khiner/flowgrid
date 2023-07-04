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

    // Get the current (concrete) store.
    // If currently in transient mode, returns a persistent version of it _without_ ending transient mode.
    StoreImpl Get() const;
    nlohmann::json GetJson() const;
    nlohmann::json GetJson(const StoreImpl &) const;

    void BeginTransient(); // End transient mode with `Commit`.

    // Overwrite the store with the provided store _if it is different_, and return the resulting (potentially empty) patch.
    Patch CheckedSet(const StoreImpl &);
    Patch CheckedSet(StoreImpl &&);

    Patch SetJson(const nlohmann::json &); // Same as above, but convert the provided JSON to a store first.
    void Commit(); // End transient mode and overwrite the store with the persistent store.
    Patch CheckedCommit();

    Primitive Get(const StorePath &) const;
    Count CountAt(const StorePath &) const;

    void Set(const StorePath &, const Primitive &) const;
    void Erase(const StorePath &) const;

    Count IdPairCount(const StorePath &) const;
    std::unordered_set<IdPair, IdPairHash> IdPairs(const StorePath &) const;
    void AddIdPair(const StorePath &, const IdPair &) const;
    void EraseIdPair(const StorePath &, const IdPair &) const;
    bool HasIdPair(const StorePath &, const IdPair &) const;

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const StoreImpl &before, const StoreImpl &after, const StorePath &base_path = RootPath) const;

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const StoreImpl &, const StorePath &base_path = RootPath) const;

    // Create a patch comparing the current transient store with the current persistent store.
    // **Ends transient mode.**
    Patch CreatePatch(const StorePath &base_path = RootPath);

private:
    void ApplyPatch(const Patch &) const;
    StoreImpl EndTransient();
    Count CheckedCommit(const StorePath &) const;

    std::unique_ptr<StoreImpl> Impl;
    std::unique_ptr<TransientStoreImpl> TransientImpl; // If this is non-null, the store is in transient mode.
};

extern Store &store;
