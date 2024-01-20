#pragma once

#include <variant>

/*
Visit a variant with lambdas, using the "overloaded pattern" described
[here](https://en.cppreference.com/w/cpp/utility/variant/visit).
The formatting that works with clang-format is:
```
std::visit(
    Match{
        [](const Menu &menu) { menu.Draw(); },
        [](const std::function<void()> &draw) { draw(); },
    },
    item
);
```

But this adds 3 lines per usage _and_ an extra level of indentation compared to what I want, which is:
```
std::visit(Match{
    [](const Menu &menu) { menu.Draw(); },
    [](const std::function<void()> &draw) { draw(); },
}, item);
```

I cannot find a way to make clang-format stop indenting this as follows:
```
std::visit(Match{
               [](const Menu &menu) { menu.Draw(); },
               [](const std::function<void()> &draw) { draw(); },
           },
           item);
```

Note that without the second argument (here, `item`), it formats as I want.
No settings of `AlignAfterOpenBracket` seem to help. Dealing with it for now.
Here is a good discussion of the root issue of nested continuation width: https://stackoverflow.com/a/75532949/780425.
It used to be a macro, but this did not work well with breakpoints, which is a bigger problem than formatting.
*/
template<typename... Ts> struct Match : Ts... {
    using Ts::operator()...;
};
template<typename... Ts> Match(Ts...) -> Match<Ts...>;

// Based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename Var> struct IsMember;
template<typename T, typename... Ts> struct IsMember<T, std::variant<Ts...>>
    : public std::disjunction<std::is_same<T, Ts>...> {};
