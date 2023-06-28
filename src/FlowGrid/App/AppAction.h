#pragma once

#include "Audio/AudioAction.h"
#include "Core/Field/FieldAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/WindowsAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Style/StyleAction.h"

DefineActionType(
    App,
    using Any = Combine<Field::Any, Store::Any, Audio::Any, FileDialog::Any, Windows::Any, Style::Any>;
);
