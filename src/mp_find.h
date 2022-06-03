#pragma once

#include <cstddef>
#include <type_traits>

/**
 * Standalone Boost's `mp_find`, from relevant parts of https://github.com/boostorg/mp11/blob/develop/include/boost/mp11/algorithm.hpp.
 * Used to get variant index by type.
 * Suggestion to use `mp_find` for this came from [here](https://stackoverflow.com/a/66386518/780425)
 */

template<std::size_t N> using mp_size_t = std::integral_constant<std::size_t, N>;

template<class L, class V>
struct mp_find_impl;

template<template<class...> class L, class V>
struct mp_find_impl<L<>, V> {
    using type = mp_size_t<0>;
};

constexpr std::size_t cx_find_index(bool const *first, bool const *last) {
    std::size_t m = 0;
    while (first != last && !*first) {
        ++m;
        ++first;
    }

    return m;
}

template<template<class...> class L, class... T, class V>
struct mp_find_impl<L<T...>, V> {
    static constexpr bool _v[] = {std::is_same<T, V>::value...};
    using type = mp_size_t<cx_find_index(_v, _v + sizeof...(T))>;
};

template<class L, class V> using mp_find = typename mp_find_impl<L, V>::type;
