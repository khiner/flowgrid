#pragma once

#include "Helper/Path.h"
#include "Patch/Patch.h"

template<typename... ValueTypes> struct TypedStoreImpl;

// Stores initialize in transient mode.
template<typename... ValueTypes> struct TypedStore {
    using ImplType = TypedStoreImpl<ValueTypes...>;

    TypedStore();
    TypedStore(const TypedStore& other) noexcept;
    TypedStore(TypedStore &&) noexcept;
    ~TypedStore();

    TypedStore &operator=(const TypedStore &other);

    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const TypedStore &before, const TypedStore &after, const StorePath &base_path);

    template<typename ValueType> const ValueType &Get(const StorePath &path) const;
    template<typename ValueType> size_t Count(const StorePath &path) const;
    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const;
    template<typename ValueType> void Erase(const StorePath &path) const;

    Patch CheckedSet(const TypedStore &store);
    void Commit();
    Patch CheckedCommit();
    Patch CreatePatch(const TypedStore &other, const StorePath &base_path = RootPath) const;
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath);

    std::unique_ptr<TypedStoreImpl<ValueTypes...>> Impl;
};
