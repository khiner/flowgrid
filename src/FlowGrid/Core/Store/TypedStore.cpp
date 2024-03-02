#include "TypedStore.h"

// Include `AddOps` function definitions for all specialized `ValueTypes` to fully implement the `CreatePatch` method.
#include "StoreDiff.h"

template<typename ValueType>
void AddOps(const auto &before, const auto &after, PatchOps &ops) {
    AddOps(before.template GetMap<ValueType>(), after.template GetMap<ValueType>(), ops);
}

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatch(const TypedStore<ValueTypes...> &before, const TypedStore<ValueTypes...> &after, ID base_component_id) {
    PatchOps ops{};
    // Use template lambda to call `AddOps` for each value type.
    ([&]<typename T>() { AddOps<T>(before, after, ops); }.template operator()<ValueTypes>(), ...);
    return {base_component_id, ops};
}
