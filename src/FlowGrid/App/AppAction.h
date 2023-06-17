#pragma once

#include "Audio/AudioAction.h"
#include "Core/Field/MatrixAction.h"
#include "Core/Field/PrimitiveAction.h"
#include "Core/Field/Vec2Action.h"
#include "Core/Field/VectorAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/WindowsAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

DefineActionType(
    App,
    using Any = Combine<Primitive::Any, Vec2::Any, Vector::Any, Matrix::Any, Store::Any, Audio::Any, FileDialog::Any, Windows::Any, Style::Any>::type;
);
