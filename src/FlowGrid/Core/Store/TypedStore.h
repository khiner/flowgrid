#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Helper/Path.h"
#include "Patch/Patch.h"

// Utility to transform a tuple of types into a tuple of types wrapped by a wrapper type.
template<template<typename> class WrapperType, typename TypesTuple> struct WrapTypes;
template<template<typename> class WrapperType, typename... Types> struct WrapTypes<WrapperType, std::tuple<Types...>> {
    using type = std::tuple<WrapperType<Types>...>;
};

template<typename ValueTypes>
struct TypedStore{
    template<typename T> using Map = immer::map<StorePath, T, PathHash>;
    template<typename T> using TransientMap = immer::map_transient<StorePath, T, PathHash>;

    using StoreMaps = typename WrapTypes<Map, ValueTypes>::type;
    using TransientStoreMaps = typename WrapTypes<TransientMap, ValueTypes>::type;

    // The store starts in transient mode.
    TypedStore() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    TypedStore(const TypedStore &other) noexcept { Set(other); }
    TypedStore(TypedStore &&other) noexcept { Set(std::move(other)); }
    ~TypedStore() = default;

    template<typename ValueType> const Map<ValueType> &GetMap() const { return std::get<Map<ValueType>>(Maps); }

    template<typename ValueType> const ValueType &Get(const StorePath &path) const { return GetTransientMap<ValueType>().at(path); }
    template<typename ValueType> size_t Count(const StorePath &path) const { return GetTransientMap<ValueType>().count(path); }

    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const { GetTransientMap<ValueType>().set(path, value); }
    template<typename ValueType> void Erase(const StorePath &path) const { GetTransientMap<ValueType>().erase(path); }

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

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const TypedStore &before, const TypedStore &after, const StorePath &base_path = RootPath) const;

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const TypedStore &store, const StorePath &base_path = RootPath) const { return CreatePatch(*this, store, base_path); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) {
        const auto patch = CreatePatch(*this, TypedStore{*this}, base_path);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
        return patch;
    }

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

    StoreMaps Persistent() const;
    TransientStoreMaps Transient() const;

    template<typename ValueType> TransientMap<ValueType> &GetTransientMap() const { return std::get<TransientMap<ValueType>>(*TransientMaps); }
};
