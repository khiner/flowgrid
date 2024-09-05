#pragma once

#include <variant>

// Visit a variant with lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
template<typename... Ts> struct Match : Ts... {
    using Ts::operator()...;
};
template<typename... Ts> Match(Ts...) -> Match<Ts...>;
