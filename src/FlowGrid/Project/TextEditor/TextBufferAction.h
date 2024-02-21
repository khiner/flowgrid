#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    TextBuffer,
    DefineUnsavedComponentAction(ShowOpenDialog, Merge, "~Open");
    DefineUnsavedComponentAction(ShowSaveDialog, Merge, "~Save as...");
    DefineComponentAction(Open, "", fs::path file_path;);
    DefineUnsavedComponentAction(Save, NoMerge, "", fs::path file_path;);

    DefineComponentAction(Set, "", std::string value;);

    // DefineUnsavedAction(Undo, NoMerge, "@cmd+z");
    // DefineUnsavedAction(Redo, NoMerge, "@shift+cmd+z");
    DefineUnsavedComponentAction(Undo, NoMerge, "");
    DefineUnsavedComponentAction(Redo, NoMerge, "");

    ComponentActionJson(Open, file_path);
    ComponentActionJson(Save, file_path);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<ShowOpenDialog, ShowSaveDialog, Save, Open, Set, Undo, Redo>;
);
