#pragma once

#include "Container/AdjacencyListAction.h"
#include "Container/NavigableAction.h"
#include "Container/SetAction.h"
#include "Container/Vec2Action.h"
#include "Container/VectorAction.h"
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
using Any = Combine<
    Bool::Any, Int::Any, UInt::Any, Float::Any, Enum::Any, Flags::Any, String::Any,
    AdjacencyList::Any, Navigable<u32>::Any, Vec2::Any, Set<u32>::Any, Vector<bool>::Any, Vector<int>::Any, Vector<u32>::Any, Vector<float>::Any, Vector<std::string>::Any,
    TextBuffer::Any>;
} // namespace Core
} // namespace Action
