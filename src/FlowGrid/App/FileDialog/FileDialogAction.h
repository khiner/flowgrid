#pragma once

#include <__filesystem/path.h>

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace fs = std::filesystem;

namespace Action {
Define(FileDialogOpen, 1, 1, Merge, "", std::string dialog_json;);
Define(FileDialogSelect, 1, 1, NoMerge, "", fs::path file_path;);
Define(FileDialogCancel, 1, 1, Merge, "");

Json(FileDialogOpen, dialog_json);
Json(FileDialogSelect, file_path);
Json(FileDialogCancel);

using FileDialogAction = ActionVariant<FileDialogOpen, FileDialogSelect, FileDialogCancel>;
} // namespace Action
