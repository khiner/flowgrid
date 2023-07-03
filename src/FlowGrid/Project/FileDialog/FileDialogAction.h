#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FileDialog,

    DefineUnsavedAction(Open, Merge, "", std::string dialog_json;);
    DefineUnsavedAction(Select, NoMerge, "", fs::path file_path;);
    // Cancel action is done by simply toggling the dialog's `Visible` field.

    using Any = ActionVariant<Open, Select>;
);
