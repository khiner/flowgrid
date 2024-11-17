#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Core/Primitive/Scalar.h"

// Utility to transform a tuple into another tuple, applying a function to each element.
template<typename OutTuple, typename InTuple, std::size_t... I>
OutTuple TransformTupleImpl(InTuple &in, auto &&func, std::index_sequence<I...>) {
    return {func(std::get<I>(in))...};
}
template<typename OutTuple, typename InTuple>
OutTuple TransformTuple(InTuple &in, auto &&func) {
    return TransformTupleImpl<OutTuple>(in, func, std::make_index_sequence<std::tuple_size_v<InTuple>>{});
}

template<typename T> using StoreMap = immer::map<ID, T>;
template<typename T> using TransientStoreMap = immer::map_transient<ID, T>;

template<typename... Ts> struct StoreBase {
    using ValueTypes = std::tuple<Ts...>;
    using StoreMaps = std::tuple<StoreMap<Ts>...>;
    using TransientStoreMaps = std::tuple<TransientStoreMap<Ts>...>;

    // The store starts in transient mode.
    StoreBase() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    StoreBase(const StoreBase &other) : Maps(other.Maps), TransientMaps(std::make_unique<TransientStoreMaps>(*other.TransientMaps)) {}
    ~StoreBase() = default;

    template<typename ValueType> ValueType At(ID id) const { return GetMap<ValueType>()[id]; }
    template<typename ValueType> ValueType Get(ID id) const { return GetTransientMap<ValueType>()[id]; }
    template<typename ValueType> size_t Count(ID id) const { return GetTransientMap<ValueType>().count(id); }

    template<typename ValueType> void Set(ID id, ValueType value) { GetTransientMap<ValueType>().set(id, std::move(value)); }
    template<typename ValueType> void Clear(ID id) { Set(id, ValueType{}); }
    template<typename ValueType> void Erase(ID id) { GetTransientMap<ValueType>().erase(id); }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Maps = Persistent(); }
    // Overwrite the persistent store with the provided store.
    void Commit(StoreMaps maps) {
        Maps = std::move(maps);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }

    StoreMaps Persistent() const {
        return TransformTuple<StoreMaps>(*TransientMaps, [](auto &map) { return map.persistent(); });
    }
    TransientStoreMaps Transient() const {
        return TransformTuple<TransientStoreMaps>(Maps, [](auto &map) { return map.transient(); });
    }

    StoreMaps Maps;
    std::unique_ptr<TransientStoreMaps> TransientMaps; // In practice, this is always non-null. todo make this a value type and fix const issues

protected:
    template<typename ValueType> auto GetMap() const { return std::get<StoreMap<ValueType>>(Maps); }
    template<typename ValueType> auto &GetTransientMap() const { return std::get<TransientStoreMap<ValueType>>(*TransientMaps); }
};
