#pragma once

#include "Core/Container/AdjacencyListAction.h"
#include "Core/Container/MultilineStringAction.h"
#include "Core/Container/Vec2Action.h"
#include "Core/Container/Vector2DAction.h"
#include "Core/Container/VectorAction.h"
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
        MultilineString::Any, Vec2::Any,
        Vector<bool>::Any, Vector<int>::Any, Vector<u32>::Any, Vector<float>::Any,
        Vector2D<bool>::Any, Vector2D<int>::Any, Vector2D<u32>::Any, Vector2D<float>::Any,
        AdjacencyList::Any>;
);
