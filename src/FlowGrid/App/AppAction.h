#pragma once

#include "Audio/AudioAction.h"
#include "Core/Stateful/Field/FieldAction.h"
#include "Core/Store/Patch/PatchAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

namespace Action {
using App = Action::Combine<Value, Values, Vector, Matrix, Patch, Audio, FileDialog, Style>::type;
} // namespace Action
