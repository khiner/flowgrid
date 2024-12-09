#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FileDialog,

    DefineAction(Open, Unsaved, Merge, "", std::string dialog_json;);
    DefineAction(Select, Unsaved, NoMerge, "", fs::path file_path;);
    // Cancel action is done by simply toggling the dialog's `Visible` field.

    using Any = ActionVariant<Open, Select>;
);
