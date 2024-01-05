#pragma once

#include "BoolAction.h"
#include "EnumAction.h"
#include "FlagsAction.h"
#include "FloatAction.h"
#include "IntAction.h"
#include "StringAction.h"
#include "UIntAction.h"

namespace Action {
namespace Primitive {
using Any = Combine<Bool::Any, Int::Any, UInt::Any, Float::Any, Enum::Any, Flags::Any, String::Any>;
} // namespace Primitive
} // namespace Action
