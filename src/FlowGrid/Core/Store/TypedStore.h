#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Helper/Path.h"
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

template<typename T> using StoreMap = immer::map<StorePath, T, PathHash>;
template<typename T> using StoreTransientMap = immer::map_transient<StorePath, T, PathHash>;

template<typename... ValueTypes> struct TypedStore {
    using StoreMaps = std::tuple<StoreMap<ValueTypes>...>;
    using TransientStoreMaps = std::tuple<StoreTransientMap<ValueTypes>...>;

    // The store starts in transient mode.
    TypedStore() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    TypedStore(const TypedStore &other) noexcept { Set(other); }
    TypedStore(TypedStore &&other) noexcept { Set(std::move(other)); }
    ~TypedStore() = default;

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const TypedStore &, const TypedStore &, const StorePath &base_path);

    template<typename ValueType> const ValueType &Get(const StorePath &path) const { return GetTransientMap<ValueType>().at(path); }
    template<typename ValueType> size_t Count(const StorePath &path) const { return GetTransientMap<ValueType>().count(path); }

    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const { GetTransientMap<ValueType>().set(path, value); }
    template<typename ValueType> void Erase(const StorePath &path) const {
        if (Count<ValueType>(path)) GetTransientMap<ValueType>().erase(path);
    }

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const TypedStore &store) {
        const auto patch = CreatePatch(store);
        Set(store);
        return patch;
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Set(*this); }
    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit() { return CheckedSet(TypedStore{*this}); }

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const TypedStore &other, const StorePath &base_path = RootPath) const { return CreatePatch(*this, other, base_path); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) {
        const auto patch = CreatePatch(*this, TypedStore{*this}, base_path);
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
