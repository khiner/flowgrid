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

template<typename... ValueTypes> struct TypedStoreImpl {
    using StoreMaps = std::tuple<StoreMap<ValueTypes>...>;
    using TransientStoreMaps = std::tuple<StoreTransientMap<ValueTypes>...>;

    // The store starts in transient mode.
    TypedStoreImpl() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    TypedStoreImpl(const TypedStoreImpl &other) noexcept { Set(other); }
    TypedStoreImpl(TypedStoreImpl &&other) noexcept { Set(std::move(other)); }
    ~TypedStoreImpl() = default;

    TypedStoreImpl &operator=(TypedStoreImpl &&other) noexcept = default;
    TypedStoreImpl &operator=(const TypedStoreImpl &other) noexcept {
        Set(other);
        return *this;
    }

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const TypedStoreImpl &before, const TypedStoreImpl &after, const StorePath &base_path);

    template<typename ValueType> const ValueType &Get(const StorePath &path) const { return GetTransientMap<ValueType>().at(path); }
    template<typename ValueType> size_t Count(const StorePath &path) const { return GetTransientMap<ValueType>().count(path); }

    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const { GetTransientMap<ValueType>().set(path, value); }
    template<typename ValueType> void Erase(const StorePath &path) const {
        if (Count<ValueType>(path)) GetTransientMap<ValueType>().erase(path);
    }

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const TypedStoreImpl &store) {
        const auto patch = CreatePatch(store);
        Set(store);
        return patch;
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Set(*this); }
    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit() { return CheckedSet(TypedStoreImpl{*this}); }

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const TypedStoreImpl &other, const StorePath &base_path = RootPath) const { return CreatePatch(*this, other, base_path); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) {
        const auto patch = CreatePatch(*this, TypedStoreImpl{*this}, base_path);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
        return patch;
    }

    template<typename ValueType> const StoreMap<ValueType> &GetMap() const { return std::get<StoreMap<ValueType>>(Maps); }

private:
    StoreMaps Maps;
    std::unique_ptr<TransientStoreMaps> TransientMaps; // If this is non-null, the store is in transient mode.

    StoreMaps Get() const { return TransientMaps ? Persistent() : Maps; }
    void Set(TypedStoreImpl &&other) noexcept {
        Maps = other.Get();
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }
    void Set(const TypedStoreImpl &other) noexcept {
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

// Stores initialize in transient mode.
template<typename... ValueTypes> struct TypedStore {
    using ImplType = TypedStoreImpl<ValueTypes...>;

    TypedStore() : Impl(std::make_unique<ImplType>()) {}
    TypedStore(const auto &other) : Impl(std::make_unique<ImplType>(*other.Impl)) {}
    TypedStore(auto &&other) : Impl(std::make_unique<ImplType>(std::move(*other.Impl))) {}
    ~TypedStore() = default;

    TypedStore &operator=(const TypedStore &other) {
        if (this != &other) { // Protect against self-assignment
            // Assuming TypedStoreImpl supports deep copying or has a copy constructor
            Impl = std::make_unique<ImplType>(*other.Impl);
        }
        return *this;
    }

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const TypedStore &before, const TypedStore &after, const StorePath &base_path) {
        return ImplType::CreatePatch(*before.Impl, *after.Impl, base_path);
    }

    template<typename ValueType> const ValueType &Get(const StorePath &path) const { return Impl->template Get<ValueType>(path); }
    template<typename ValueType> size_t Count(const StorePath &path) const { return Impl->template Count<ValueType>(path); }
    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const { Impl->template Set<ValueType>(path, value); }
    template<typename ValueType> void Erase(const StorePath &path) const { Impl->template Erase<ValueType>(path); }

    Patch CheckedSet(const TypedStore &store) { return Impl->CheckedSet(*store.Impl); }
    void Commit() { Impl->Commit(); }
    Patch CheckedCommit() { return Impl->CheckedCommit(); }
    Patch CreatePatch(const TypedStore &other, const StorePath &base_path = RootPath) const { return Impl->CreatePatch(*other.Impl, base_path); }
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) { return Impl->CreatePatchAndResetTransient(base_path); }

    std::unique_ptr<TypedStoreImpl<ValueTypes...>> Impl;
};
