#pragma once

#include <variant>

// Utility to visit a variant with lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
// E.g. `Match(action, [](const ProjectAction &a) { ... }, [](const StatefulAction &a) { ... });`
template<class... Ts> struct visitor : Ts... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

// Utility to flatten two variants together into one variant.
// Based on https://stackoverflow.com/a/59251342/780425, but adds support for > 2 variants using template recursion.
// E.g. `Combine<Variant1, Variant2, Variant3>`
template<typename... Vars> struct Combine;
template<typename Var1> struct Combine<Var1> {
    using type = Var1;
};
template<typename... Ts1, typename... Ts2, typename... Vars> struct Combine<std::variant<Ts1...>, std::variant<Ts2...>, Vars...> {
    using type = typename Combine<std::variant<Ts1..., Ts2...>, Vars...>::type;
};

// `IsVariantMember` is based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename Var> struct IsVariantMember;
template<typename T, typename... Ts> struct IsVariantMember<T, std::variant<Ts...>>
    : public std::disjunction<std::is_same<T, Ts>...> {};

/**
Get variant index by type.
Example usage:
```
    template<typename T> constexpr size_t SomeVariantId = VariantIndex<T, SomeVariantType>::value;
    size_t id = SomeVariantId<SomeVariantMemberType>;
```

How I got here:
 * - Found suggestion to use `mp_find` to find variant index by type [here](https://stackoverflow.com/a/66386518/780425).
 * - Started with a minimal standalone copy of Boost's `mp_find`, from relevant parts of https://github.com/boostorg/mp11/blob/develop/include/boost/mp11/algorithm.hpp.
 * - Coaxed GPT-4 to help simplify and modernize it, which removed more than half the lines, as well as the dependency on `type_traits` and `cstddef` :)
*/
template<typename T, typename Var> struct VariantIndex;
template<typename T, typename... Ts> struct VariantIndex<T, std::variant<T, Ts...>> {
    static constexpr size_t value = 0;
};
template<typename T, typename U, typename... Ts> struct VariantIndex<T, std::variant<U, Ts...>> {
    static constexpr size_t value = 1 + VariantIndex<T, std::variant<Ts...>>::value;
};
