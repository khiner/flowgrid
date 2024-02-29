#include "TypedStore.h"

// Here, we include `AddOps` function definitions for all specialized `ValueTypes`,
// to fully implement the `CreatePatch` method.
#include "StoreOps.h"

template<typename ValueType>
void AddOps(const auto &before, const auto &after, const StorePath &base, PatchOps &ops) {
    AddOps(before.template GetMap<ValueType>(), after.template GetMap<ValueType>(), base, ops);
}

template<typename... ValueTypes>
Patch TypedStoreImpl<ValueTypes...>::CreatePatch(const TypedStoreImpl<ValueTypes...> &before, const TypedStoreImpl<ValueTypes...> &after, const StorePath &base) {
    PatchOps ops{};
    // Use template lambda to call `AddOps` for each value type.
    ([&]<typename T>() { AddOps<T>(before, after, base, ops); }.template operator()<ValueTypes>(), ...);
    return {ops, base};
}
