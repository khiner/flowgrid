#pragma once

#include "Core/Field/MatrixAction.h"
#include "Core/Field/PrimitiveAction.h"
#include "Core/Field/Vec2Action.h"
#include "Core/Field/VectorAction.h"

DefineActionType(
    Field,
    using Any = Combine<Primitive::Any, Vec2::Any, Vector::Any, Matrix::Any>::type;
);
