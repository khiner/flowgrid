#pragma once

#include "Core/Container/AdjacencyListAction.h"
#include "Core/Container/NavigableAction.h"
#include "Core/Container/PrimitiveVector2DAction.h"
#include "Core/Container/PrimitiveVectorAction.h"
#include "Core/Container/TextBufferAction.h"
#include "Core/Container/Vec2Action.h"
#include "Core/Primitive/PrimitiveAction.h"

DefineActionType(
    Field,
    using Any = Combine<
        Primitive::Any, TextBuffer::Any, Vec2::Any,
        PrimitiveVector<bool>::Any, PrimitiveVector<int>::Any, PrimitiveVector<u32>::Any, PrimitiveVector<float>::Any, PrimitiveVector<std::string>::Any,
        PrimitiveVector2D<bool>::Any, PrimitiveVector2D<int>::Any, PrimitiveVector2D<u32>::Any, PrimitiveVector2D<float>::Any,
        AdjacencyList::Any, Navigable<u32>::Any>;
);
