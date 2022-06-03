#pragma once

#include <cstddef>
#include <type_traits>

/**
 * Standalone implementation of Boost's `mp_find`.
 * Copied relevant parts from https://github.com/boostorg/mp11/blob/develop/include/boost/mp11/algorithm.hpp
 * This is used to get variant index by type.
 * Suggestion to use `mp_find` to do this [here](https://stackoverflow.com/a/66386518/780425)
 * TODO get rid of more unused paths here
 */

template<std::size_t N> using mp_size_t = std::integral_constant<std::size_t, N>;

template<class L, class V>
struct mp_find_impl;

#if !defined( BOOST_MP11_NO_CONSTEXPR )

template<template<class...> class L, class V>
struct mp_find_impl<L<>, V> {
    using type = mp_size_t<0>;
};

#if defined( BOOST_MP11_HAS_CXX14_CONSTEXPR )

constexpr std::size_t cx_find_index(bool const *first, bool const *last) {
    std::size_t m = 0;
    while (first != last && !*first) {
        ++m;
        ++first;
    }

    return m;
}

#else

constexpr std::size_t cx_find_index(bool const *first, bool const *last) {
    return first == last || *first ? 0 : 1 + cx_find_index(first + 1, last);
}

#endif

template<template<class...> class L, class... T, class V>
struct mp_find_impl<L<T...>, V> {
    static constexpr bool _v[] = {std::is_same<T, V>::value...};
    using type = mp_size_t<cx_find_index(_v, _v + sizeof...(T))>;
};

#else

template<template<class...> class L, class V>
struct mp_find_impl<L<>, V> {
    using type = mp_size_t<0>;
};

template<template<class...> class L, class... T, class V>
struct mp_find_impl<L<V, T...>, V> {
    using type = mp_size_t<0>;
};

template<template<class...> class L, class T1, class... T, class V>
struct mp_find_impl<L<T1, T...>, V> {
    using _r = typename mp_find_impl<mp_list<T...>, V>::type;
    using type = mp_size_t<1 + _r::value>;
};

#endif

template<class L, class V> using mp_find = typename mp_find_impl<L, V>::type;
