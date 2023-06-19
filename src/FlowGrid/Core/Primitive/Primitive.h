#pragma once

#include <concepts>
#include <string>
#include <vector>

#include "Scalar.h"
#include "Helper/Variant.h"

using Primitive = std::variant<bool, U32, S32, float, std::string>;

template<typename T>
concept IsPrimitive = Variant::IsMember<T, Primitive>::value;
