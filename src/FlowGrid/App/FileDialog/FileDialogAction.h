#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Json.h"

namespace Action {
using namespace Actionable;

Define(OpenFileDialog, 1, 1, Merge, "", std::string dialog_json;);
Define(CloseFileDialog, 1, 1, Merge, "");

Json(CloseFileDialog);
Json(OpenFileDialog, dialog_json);

using FileDialogAction = ActionVariant<OpenFileDialog, CloseFileDialog>;
} // namespace Action
