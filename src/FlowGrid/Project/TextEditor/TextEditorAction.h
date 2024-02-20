#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    TextEditor,
    DefineComponentAction(Set, "", std::string value;);
    DefineUnsavedComponentAction(ShowOpenDialog, Merge, "~Open");
    DefineUnsavedComponentAction(ShowSaveDialog, Merge, "~Save as...");
    DefineComponentAction(Open, "", fs::path file_path;);
    DefineUnsavedComponentAction(Save, NoMerge, "", fs::path file_path;);

    ComponentActionJson(Set, value);
    ComponentActionJson(Open, file_path);
    ComponentActionJson(Save, file_path);

    using Any = ActionVariant<Set, ShowOpenDialog, ShowSaveDialog, Save, Open>;
);
