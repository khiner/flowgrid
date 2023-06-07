#pragma once

#include "Audio/AudioAction.h"
#include "Core/Store/StoreAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

namespace Action {
using App = Action::Combine<Audio, FileDialog, Style, Store>::type;
} // namespace Action
