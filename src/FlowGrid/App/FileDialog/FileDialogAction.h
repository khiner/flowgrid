#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FileDialog,

    DefineAction(Open, Merge, "", std::string dialog_json;);
    DefineAction(Select, NoMerge, "", fs::path file_path;);
    // Cancel action is done by simply toggling the dialog's `Visible` field.

    Json(Open, dialog_json);
    Json(Select, file_path);

    using Any = ActionVariant<Open, Select>;
);
