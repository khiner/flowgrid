#pragma once

#include "Audio/AudioAction.h"
#include "Core/Field/FieldAction.h"
#include "Core/Store/Patch/PatchAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

namespace Action {
using App = Action::Combine<Primitive, Vector, Matrix, Patch, Audio, FileDialog, Style>::type;
} // namespace Action
