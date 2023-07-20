#pragma once

#include "Core/Container/AdjacencyListAction.h"
#include "Core/Container/PrimitiveVector2DAction.h"
#include "Core/Container/PrimitiveVectorAction.h"
#include "Core/Container/TextBufferAction.h"
#include "Core/Container/Vec2Action.h"
#include "Core/Primitive/BoolAction.h"
#include "Core/Primitive/EnumAction.h"
#include "Core/Primitive/FlagsAction.h"
#include "Core/Primitive/FloatAction.h"
#include "Core/Primitive/IntAction.h"
#include "Core/Primitive/StringAction.h"
#include "Core/Primitive/UIntAction.h"

DefineActionType(
    Field,
    using Any = Combine<
        Primitive::Bool::Any, Primitive::Int::Any, Primitive::UInt::Any, Primitive::Float::Any, Primitive::String::Any, Primitive::Enum::Any, Primitive::Flags::Any,
        TextBuffer::Any, Vec2::Any,
        PrimitiveVector<bool>::Any, PrimitiveVector<int>::Any, PrimitiveVector<u32>::Any, PrimitiveVector<float>::Any,
        PrimitiveVector2D<bool>::Any, PrimitiveVector2D<int>::Any, PrimitiveVector2D<u32>::Any, PrimitiveVector2D<float>::Any,
        AdjacencyList::Any>;
);
