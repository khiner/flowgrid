#pragma once

#include "nlohmann/json.hpp"

#include "Patch.h"
#include "StoreEntries.h"

struct Store;

namespace store {
void BeginTransient(); // End transient mode with `Commit`.

const Store &Get(); // Get a read-only reference to the canonical application store.

nlohmann::json GetJson();
nlohmann::json GetJson(const Store &);

Store GetPersistent(); // Get the persistent store from the transient store _without_ ending transient mode.
Patch CheckedSet(const Store &); // Overwrite the store with the provided store _if it is different_, and return the resulting (potentially empty) patch.
Patch CheckedSetJson(const nlohmann::json &); // Same as above, but convert the provided JSON to a store first.

void Commit(); // End transient mode and overwrite the store with the persistent store.
Patch CheckedCommit();

Primitive Get(const StorePath &);
Count CountAt(const StorePath &);

void Set(const StorePath &, const Primitive &);
void Set(const StoreEntries &);
void Set(const StorePath &, const std::vector<Primitive> &);
void Set(const StorePath &, const std::vector<Primitive> &, Count row_count); // For `SetMatrix` action.

void Erase(const StorePath &);

// Create a patch comparing the provided stores.
Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath);
// Create a patch comparing the provided store with the current persistent store.
Patch CreatePatch(const Store &, const StorePath &base_path = RootPath);
// Create a patch comparing the current transient store with the current persistent store.
// **Stops transient mode.**
Patch CreatePatch(const StorePath &base_path = RootPath);

void ApplyPatch(const Patch &);

} // namespace store
