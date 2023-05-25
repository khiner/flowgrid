#pragma once

#include "StoreFwd.h"

#include "../StateMember.h"

using std::vector;

namespace store {
void OnApplicationStateInitialized();

void BeginTransient();
const Store EndTransient(bool commit = true);
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

Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath);
Patch CreatePatch(const StorePath &base_path = RootPath); // Create a patch from the current transient store (stops transient mode).

void ApplyPatch(const Patch &);

} // namespace store
