#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FileDialog,

    DefineAction(Open, Merge, "", std::string dialog_json;);
    DefineAction(Select, NoMerge, "", fs::path file_path;);
    DefineAction(Cancel, NoMerge, "");

    Json(Open, dialog_json);
    Json(Select, file_path);
    Json(Cancel);

    using Any = ActionVariant<Open, Select, Cancel>;
);
