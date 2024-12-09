#pragma once

#include "Container/ContainerAction.h"
#include "Primitive/BoolAction.h"
#include "Primitive/EnumAction.h"
#include "Primitive/FlagsAction.h"
#include "Primitive/FloatAction.h"
#include "Primitive/IntAction.h"
#include "Primitive/StringAction.h"
#include "Primitive/UIntAction.h"
#include "TextEditor/TextBufferAction.h"

namespace Action {
namespace Core {
using Any = Combine<Bool::Any, Int::Any, UInt::Any, Float::Any, Enum::Any, Flags::Any, String::Any, Container::Any, TextBuffer::Any>;
} // namespace Core
} // namespace Action
