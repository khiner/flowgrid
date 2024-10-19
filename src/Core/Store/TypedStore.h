#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "IDs.h"
#include "Patch/Patch.h"

// Utility to transform a tuple into another tuple, applying a function to each element.
template<typename ResultTuple, typename InputTuple, typename Func, std::size_t... I>
ResultTuple TransformTupleImpl(InputTuple &in, Func func, std::index_sequence<I...>) {
    return {func(std::get<I>(in))...};
}
template<typename ResultTuple, typename InputTuple, typename Func>
ResultTuple TransformTuple(InputTuple &in, Func func) {
    return TransformTupleImpl<ResultTuple>(in, func, std::make_index_sequence<std::tuple_size_v<InputTuple>>{});
}

template<typename T> using StoreMap = immer::map<ID, T>;
template<typename T> using TransientStoreMap = immer::map_transient<ID, T>;

template<typename... ValueTypes> struct TypedStore {
    using StoreMaps = std::tuple<StoreMap<ValueTypes>...>;
    using TransientStoreMaps = std::tuple<TransientStoreMap<ValueTypes>...>;

    // The store starts in transient mode.
    TypedStore() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    TypedStore(const TypedStore &other) : Maps(other.Maps), TransientMaps(std::make_unique<TransientStoreMaps>(*other.TransientMaps)) {}
    ~TypedStore() = default;

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const StoreMaps &, const StoreMaps &, ID base_component_id);

    template<typename ValueType> const ValueType &Get(ID id) const { return GetTransientMap<ValueType>()[id]; }
    template<typename ValueType> size_t Count(ID id) const { return GetTransientMap<ValueType>().count(id); }

    template<typename ValueType> void Set(ID id, const ValueType &value) { GetTransientMap<ValueType>().set(id, value); }
    template<typename ValueType> void Clear(ID id) { Set(id, ValueType{}); }
    template<typename ValueType> void Erase(ID id) {
        if (Count<ValueType>(id)) GetTransientMap<ValueType>().erase(id);
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Maps = Persistent(); }
    // Overwrite the persistent store with the provided store.
    void Commit(StoreMaps maps) {
        Maps = std::move(maps);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }

    // Same as `Commit`, but returns thtatic e resulting patch.
    Patch CheckedCommit(ID base_component_id) {
        auto new_maps = Persistent();
        const auto patch = CreatePatch(Maps, new_maps, base_component_id);
        Commit(std::move(new_maps));
        return patch;
    }

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const TypedStore &other, ID base_component_id) const { return CreatePatch(Maps, other.Maps, base_component_id); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(ID base_component_id) {
        const auto patch = CreatePatch(Maps, Persistent(), base_component_id);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
        return patch;
    }

    StoreMaps Persistent() const {
        return TransformTuple<StoreMaps>(*TransientMaps, [](auto &map) { return map.persistent(); });
    }
    TransientStoreMaps Transient() const {
        return TransformTuple<TransientStoreMaps>(Maps, [](auto &map) { return map.transient(); });
    }

    template<typename ValueType> TransientStoreMap<ValueType> &GetTransientMap() const { return std::get<TransientStoreMap<ValueType>>(*TransientMaps); }

    StoreMaps Maps;
    std::unique_ptr<TransientStoreMaps> TransientMaps; // In practice, this is always non-null. todo make this a value type and fix const issues
};
