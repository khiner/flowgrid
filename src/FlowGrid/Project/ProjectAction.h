#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Project,
    DefineUnsavedAction(ShowOpenDialog, Merge, "~Open project");
    DefineUnsavedAction(ShowSaveDialog, Merge, "~Save project as...");

    DefineUnsavedAction(Undo, NoMerge, "");
    DefineUnsavedAction(Redo, NoMerge, "");
    DefineUnsavedAction(SetHistoryIndex, NoMerge, "", u32 index;);
    DefineUnsavedAction(Open, NoMerge, "", fs::path file_path;);
    DefineUnsavedAction(OpenEmpty, NoMerge, "~New project");
    DefineUnsavedAction(OpenDefault, NoMerge, "");
    DefineUnsavedAction(Save, NoMerge, "", fs::path file_path;);
    DefineUnsavedAction(SaveDefault, NoMerge, "");
    DefineUnsavedAction(SaveCurrent, NoMerge, "~Save project");

    using Any = ActionVariant<
        Undo, Redo, SetHistoryIndex,
        Open, OpenEmpty, OpenDefault, Save, SaveDefault, SaveCurrent, ShowOpenDialog, ShowSaveDialog>;
);
