#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"
#include "Helper/Path.h"

namespace Action {
Define(FileDialogOpen, Merge, "", std::string dialog_json;);
Define(FileDialogSelect, NoMerge, "", fs::path file_path;);
Define(FileDialogCancel, Merge, "");

Json(FileDialogOpen, dialog_json);
Json(FileDialogSelect, file_path);
Json(FileDialogCancel);

using FileDialog = ActionVariant<FileDialogOpen, FileDialogSelect, FileDialogCancel>;
} // namespace Action
