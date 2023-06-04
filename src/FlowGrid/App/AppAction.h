#pragma once

#include "Audio/AudioAction.h"
#include "Core/Store/StoreAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

namespace Action {
using AppAction = Action::Combine<AudioAction, FileDialogAction, StyleAction, StoreAction>::type;
} // namespace Action
