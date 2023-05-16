#pragma once

#include <variant>

// Utility to make a variant visitor out of lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
template<class... Ts> struct visitor : Ts... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

// E.g. Match(action, [](const ProjectAction &a) { ... }, [](const StatefulAction &a) { ... });
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

// Utility to flatten two variants together into one variant.
// Based on https://stackoverflow.com/a/59251342/780425, but adds support for > 2 variants using template recursion.
// E.g. Combine<Variant1, Variant2, Variant3>
template<typename... Vars>
struct Combine;

template<typename Var1>
struct Combine<Var1> {
    using type = Var1;
};

template<typename... Ts1, typename... Ts2, typename... Vars>
struct Combine<std::variant<Ts1...>, std::variant<Ts2...>, Vars...> {
    using type = typename Combine<std::variant<Ts1..., Ts2...>, Vars...>::type;
};
