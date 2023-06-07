#pragma once

#include "Audio/AudioAction.h"
#include "Core/Stateful/FieldAction.h"
#include "Core/Store/PatchAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

namespace Action {
using App = Action::Combine<Value, Values, Vector, Matrix, Patch, Audio, FileDialog, Style>::type;
} // namespace Action
