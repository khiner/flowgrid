#pragma once

#include "Audio/AudioAction.h"
#include "Core/Field/FieldAction.h"
#include "Core/Store/Patch/PatchAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

DefineActionType(
    App,
    using Any = Combine<Primitive::Any, Vector::Any, Matrix::Any, Patch::Any, Audio::Any, FileDialog::Any, Style::Any>::type;
);
