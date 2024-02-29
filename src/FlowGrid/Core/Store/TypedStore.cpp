#include "TypedStore.h"

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

// `TypedStoreImpl`

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

// `TypedStore`

template<typename... ValueTypes>
TypedStore<ValueTypes...> &TypedStore<ValueTypes...>::operator=(const TypedStore<ValueTypes...> &other) {
    if (this != &other) { // Protect against self-assignment
        // Assuming TypedStoreImpl supports deep copying or has a copy constructor
        Impl = std::make_unique<ImplType>(*other.Impl);
    }
    return *this;
}

// Constructors
template<typename... ValueTypes>
TypedStore<ValueTypes...>::TypedStore() : Impl(std::make_unique<TypedStoreImpl<ValueTypes...>>()) {}

template<typename... ValueTypes>
TypedStore<ValueTypes...>::TypedStore(const TypedStore& other) noexcept : Impl(std::make_unique<TypedStoreImpl<ValueTypes...>>(*other.Impl)) {}

template<typename... ValueTypes>
TypedStore<ValueTypes...>::TypedStore(TypedStore<ValueTypes...> &&other) noexcept : Impl(std::move(other.Impl)) {}

// Destructor
template<typename... ValueTypes>
TypedStore<ValueTypes...>::~TypedStore() {}

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatch(const TypedStore<ValueTypes...> &before, const TypedStore<ValueTypes...> &after, const StorePath &base_path) {
    return ImplType::CreatePatch(*before.Impl, *after.Impl, base_path);
}

template<typename... ValueTypes>
template<typename ValueType>
const ValueType &TypedStore<ValueTypes...>::Get(const StorePath &path) const { return Impl->template Get<ValueType>(path); }

template<typename... ValueTypes>
template<typename ValueType>
size_t TypedStore<ValueTypes...>::Count(const StorePath &path) const { return Impl->template Count<ValueType>(path); }

template<typename... ValueTypes>
template<typename ValueType>
void TypedStore<ValueTypes...>::Set(const StorePath &path, const ValueType &value) const { Impl->template Set<ValueType>(path, value); }

template<typename... ValueTypes>
template<typename ValueType>
void TypedStore<ValueTypes...>::Erase(const StorePath &path) const { Impl->template Erase<ValueType>(path); }

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CheckedSet(const TypedStore<ValueTypes...> &store) { return Impl->CheckedSet(*store.Impl); }

template<typename... ValueTypes>
void TypedStore<ValueTypes...>::Commit() { Impl->Commit(); }

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CheckedCommit() { return Impl->CheckedCommit(); }

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatch(const TypedStore &other, const StorePath &base_path) const { return Impl->CreatePatch(*other.Impl, base_path); }

template<typename... ValueTypes>
Patch TypedStore<ValueTypes...>::CreatePatchAndResetTransient(const StorePath &base_path) { return Impl->CreatePatchAndResetTransient(base_path); }

// Utility to transform a tuple into another tuple, applying a function to each element.
template<typename ResultTuple, typename InputTuple, typename Func, std::size_t... I>
ResultTuple TransformTupleImpl(InputTuple &in, Func func, std::index_sequence<I...>) {
    return {func(std::get<I>(in))...};
}
template<typename ResultTuple, typename InputTuple, typename Func>
ResultTuple TransformTuple(InputTuple &in, Func func) {
    return TransformTupleImpl<ResultTuple>(in, func, std::make_index_sequence<std::tuple_size_v<InputTuple>>{});
}


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
