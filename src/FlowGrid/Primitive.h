#pragma once

#include <concepts>
#include <string>
#include <variant>
#include <vector>

#include "Scalar.h"

using std::string;

using Primitive = std::variant<bool, U32, S32, float, string>;

// `IsVariantMember` is based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename VARIANT_T>
struct IsVariantMember;

template<typename T, typename... ALL_T>
struct IsVariantMember<T, std::variant<ALL_T...>>
    : public std::disjunction<std::is_same<T, ALL_T>...> {};

template<typename T>
concept IsPrimitive = IsVariantMember<T, Primitive>::value;
