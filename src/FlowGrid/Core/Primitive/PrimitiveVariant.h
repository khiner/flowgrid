#pragma once

#include <concepts>
#include <string>

#include "Helper/Variant.h"
#include "Scalar.h"

using PrimitiveVariant = std::variant<bool, u32, s32, float, std::string>;
