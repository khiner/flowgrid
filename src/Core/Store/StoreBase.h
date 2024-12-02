#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Core/ID.h"

// Utility to transform a tuple into another tuple, applying a function to each element.
auto TransformTuple(auto &&in, auto &&func) noexcept {
    return std::apply(
        [&func](auto &&...elements) {
            return std::make_tuple(func(std::forward<decltype(elements)>(elements))...);
        },
        std::forward<decltype(in)>(in)
    );
}

template<typename T> using StoreMap = immer::map<ID, T>;
template<typename T> using TransientStoreMap = immer::map_transient<ID, T>;

template<typename... Ts> struct TransientStoreMaps;

template<typename... Ts> struct StoreMaps {
    using MapsT = std::tuple<StoreMap<Ts>...>;
    using ValuesT = std::tuple<Ts...>;

    template<typename T> const T &Get(ID id) const { return GetMap<T>()[id]; }

    template<typename T> decltype(auto) GetMap() const { return std::get<StoreMap<T>>(Maps); }

    TransientStoreMaps<Ts...> Transient() const {
        return {TransformTuple(Maps, [](auto &&map) { return map.transient(); })};
    }

    MapsT Maps;
};

template<typename... Ts> struct TransientStoreMaps {
    using MapsT = std::tuple<TransientStoreMap<Ts>...>;

    template<typename T> const T &Get(ID id) const { return GetMap<T>()[id]; }
    template<typename T> size_t Count(ID id) const { return GetMap<T>().count(id); }

    template<typename T> void Set(ID id, T value) { GetMap<T>().set(id, std::move(value)); }
    template<typename T> void Clear(ID id) { Set(id, T{}); }
    template<typename T> void Erase(ID id) { GetMap<T>().erase(id); }

    // Deduced-this for const/non-const overloads.
    template<typename T> decltype(auto) GetMap(this auto &&self) {
        return std::get<TransientStoreMap<T>>(self.Maps);
    }

    StoreMaps<Ts...> Persistent() {
        return {TransformTuple(Maps, [](auto &&map) { return map.persistent(); })};
    }

    MapsT Maps;
};
