#pragma once

#include "AdjacencyListAction.h"
#include "NavigableAction.h"
#include "PrimitiveVector2DAction.h"
#include "PrimitiveVectorAction.h"
#include "TextBufferAction.h"
#include "Vec2Action.h"

namespace Action {
namespace Container {
using Any = Combine<TextBuffer::Any, Vec2::Any,
        PrimitiveVector<bool>::Any, PrimitiveVector<int>::Any, PrimitiveVector<u32>::Any, PrimitiveVector<float>::Any, PrimitiveVector<std::string>::Any,
        PrimitiveVector2D<bool>::Any, PrimitiveVector2D<int>::Any, PrimitiveVector2D<u32>::Any, PrimitiveVector2D<float>::Any,
        AdjacencyList::Any, Navigable<u32>::Any>;
} // namespace Primitive
} // namespace Action
