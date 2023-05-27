#pragma once

#include "StoreFwd.h"

#include "Core/Action/Action.h"
#include "Core/Stateful/StateMember.h"

using std::vector;

namespace store {
void Apply(const Action::StoreAction &);

void BeginTransient();
const Store EndTransient();
void CommitTransient();
TransientStore &GetTransient(); // xxx temporary until all sets are done internally.
bool IsTransientMode();
Store GetPersistent();

Primitive Get(const StorePath &);
Count CountAt(const StorePath &);

void Set(const StorePath &, const Primitive &);
void Set(const StoreEntries &);
void Set(const StorePath &, const vector<Primitive> &);
void Set(const StorePath &, const vector<Primitive> &, Count row_count); // For `SetMatrix` action.

void Erase(const StorePath &);

// Overwrite the main application store.
// This is the only place `ApplicationStore` is modified.
void Set(const Store &);

// Create a patch comparing the provided stores.
Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath);
// Create a patch comparing the provided store with the current persistent store.
Patch CreatePatch(const Store &, const StorePath &base_path = RootPath);
// Create a patch comparing the current transient store with the current persistent store. (Stops transient mode.)
Patch CreatePatch(const StorePath &base_path = RootPath);

void ApplyPatch(const Patch &);

} // namespace store
