#pragma once

#include <variant>

// Utility to visit a variant with lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
// E.g. `Match(variant_instance, [](const VariantType1 &v) { ... }, [](const VariantType2 &v) { ... });`
template<class... Ts> struct visitor : Ts... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

namespace Variant {
// `Variant::IsMember` is based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename Var> struct IsMember;
template<typename T, typename... Ts> struct IsMember<T, std::variant<Ts...>>
    : public std::disjunction<std::is_same<T, Ts>...> {};
} // namespace Variant
