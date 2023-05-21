#pragma once

#include <stdexcept>
#include <string>
#include <variant>

// Utility to visit a variant with lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
// E.g. `Match(action, [](const ProjectAction &a) { ... }, [](const StatefulAction &a) { ... });`
template<class... Ts> struct visitor : Ts... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

namespace Variant {
// Utility to flatten two variants together into one variant.
// Based on https://stackoverflow.com/a/59251342/780425, but adds support for > 2 variants using template recursion.
// E.g. `Variant::Combine<Variant1, Variant2, Variant3>`
template<typename... Vars> struct Combine;
template<typename Var> struct Combine<Var> {
    using type = Var;
};
template<typename... Ts1, typename... Ts2, typename... Vars> struct Combine<std::variant<Ts1...>, std::variant<Ts2...>, Vars...> {
    using type = typename Combine<std::variant<Ts1..., Ts2...>, Vars...>::type;
};

// `Variant::IsMember` is based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename Var> struct IsMember;
template<typename T, typename... Ts> struct IsMember<T, std::variant<Ts...>>
    : public std::disjunction<std::is_same<T, Ts>...> {};

/**
Get variant index by type. Example usage:
```
    template<typename T> constexpr size_t SomeVariantIndex = Variant::Index<T, SomeVariantType>::value;
    constexpr size_t id = SomeVariantIndex<SomeVariantMemberType>;
```
*/
template<typename T, typename Var> struct Index;
template<typename T, typename... Ts> struct Index<T, std::variant<T, Ts...>> {
    static constexpr size_t value = 0;
};
template<typename T, typename U, typename... Ts> struct Index<T, std::variant<U, Ts...>> {
    static constexpr size_t value = 1 + Index<T, std::variant<Ts...>>::value;
};

// Default-construct a variant member by its index.
// Adapted from: https://stackoverflow.com/a/60567091/780425
template<typename Var, size_t I = 0> Var Create(size_t index) {
    if constexpr (I >= std::variant_size_v<Var>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
    else return index == 0 ? Var{std::in_place_index<I>} : Create<Var, I + 1>(index - 1);
}
} // namespace Variant
