#pragma once

#include <unordered_set>

#include "nlohmann/json.hpp"

#include "Core/Action/Actionable.h"
#include "IdPair.h"
#include "Patch/Patch.h"
#include "StoreAction.h"

struct StoreImpl;

struct Store {
    struct ActionHandler : Actionable<Action::Store::Any> {
        void Apply(const ActionType &) const override;
        bool CanApply(const ActionType &) const override { return true; }
    };

    inline static ActionHandler ActionHandler;

    void BeginTransient() const; // End transient mode with `Commit`.

    const StoreImpl &Get() const; // Get a read-only reference to the canonical project store.

    nlohmann::json GetJson() const;
    nlohmann::json GetJson(const StoreImpl &) const;

    StoreImpl GetPersistent() const; // Get the persistent store from the transient store _without_ ending transient mode.
    Patch CheckedSet(const StoreImpl &) const; // Overwrite the store with the provided store _if it is different_, and return the resulting (potentially empty) patch.
    Patch SetJson(const nlohmann::json &) const; // Same as above, but convert the provided JSON to a store first.

    void Commit() const; // End transient mode and overwrite the store with the persistent store.
    Patch CheckedCommit() const;

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
    // **Stops transient mode.**
    Patch CreatePatch(const StorePath &base_path = RootPath) const;

private:
    void ApplyPatch(const Patch &) const;
    StoreImpl EndTransient() const;
    Count CheckedCommit(const StorePath &) const;
};

extern const Store &store;
