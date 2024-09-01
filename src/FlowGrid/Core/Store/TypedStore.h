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
template<typename T> using StoreTransientMap = immer::map_transient<ID, T>;

template<typename... ValueTypes> struct TypedStore {
    using StoreMaps = std::tuple<StoreMap<ValueTypes>...>;
    using TransientStoreMaps = std::tuple<StoreTransientMap<ValueTypes>...>;

    // The store starts in transient mode.
    TypedStore() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    TypedStore(const TypedStore &other) noexcept { Set(other); }
    TypedStore(TypedStore &&other) noexcept { Set(std::move(other)); }
    ~TypedStore() = default;

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const TypedStore &, const TypedStore &, ID base_component_id);

    template<typename ValueType> const ValueType &Get(ID id) const { return GetTransientMap<ValueType>()[id]; }
    template<typename ValueType> size_t Count(ID id) const { return GetTransientMap<ValueType>().count(id); }

    template<typename ValueType> void Set(ID id, const ValueType &value) const { GetTransientMap<ValueType>().set(id, value); }
    template<typename ValueType> void Clear(ID id) const { Set(id, ValueType{}); }
    template<typename ValueType> void Erase(ID id) const {
        if (Count<ValueType>(id)) GetTransientMap<ValueType>().erase(id);
    }

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const TypedStore &store, ID base_component_id) {
        const auto patch = CreatePatch(store, base_component_id);
        Set(store);
        return patch;
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Set(*this); }
    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit(ID base_component_id) { return CheckedSet(TypedStore{*this}, base_component_id); }

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const TypedStore &other, ID base_component_id) const { return CreatePatch(*this, other, base_component_id); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(ID base_component_id) {
        const auto patch = CreatePatch(*this, TypedStore{*this}, base_component_id);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
        return patch;
    }

    template<typename ValueType> const StoreMap<ValueType> &GetMap() const { return std::get<StoreMap<ValueType>>(Maps); }

private:
    StoreMaps Maps;
    std::unique_ptr<TransientStoreMaps> TransientMaps; // If this is non-null, the store is in transient mode.

    StoreMaps Get() const { return TransientMaps ? Persistent() : Maps; }
    void Set(TypedStore &&other) noexcept {
        Maps = other.Get();
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }
    void Set(const TypedStore &other) noexcept {
        Maps = other.Get();
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }
    StoreMaps Persistent() const {
        if (!TransientMaps) throw std::runtime_error("Store is not in transient mode.");
        return TransformTuple<StoreMaps>(*TransientMaps, [](auto &map) { return map.persistent(); });
    }
    TransientStoreMaps Transient() const {
        return TransformTuple<TransientStoreMaps>(Maps, [](auto &map) { return map.transient(); });
    }

    template<typename ValueType> StoreTransientMap<ValueType> &GetTransientMap() const { return std::get<StoreTransientMap<ValueType>>(*TransientMaps); }
};
