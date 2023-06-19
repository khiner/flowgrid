#pragma once

#include <concepts>
#include <string>
#include <vector>

#include "Helper/Variant.h"
#include "Scalar.h"

using Primitive = std::variant<bool, U32, S32, float, std::string>;

template<typename T>
concept IsPrimitive = Variant::IsMember<T, Primitive>::value;
