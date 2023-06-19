#pragma once

#include "Core/Container/MatrixAction.h"
#include "Core/Container/Vec2Action.h"
#include "Core/Container/VectorAction.h"
#include "Core/Primitive/PrimitiveAction.h"

DefineActionType(
    Field,
    using Any = Combine<Primitive::Any, Vec2::Any, Vector::Any, Matrix::Any>::type;
);
