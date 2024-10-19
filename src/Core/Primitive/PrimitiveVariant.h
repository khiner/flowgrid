#pragma once

#include <concepts>
#include <string>
#include <variant>

#include "Scalar.h"

using PrimitiveVariant = std::variant<bool, u32, s32, float, std::string>;
