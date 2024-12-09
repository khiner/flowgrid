#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Project,
    DefineAction(ShowOpenDialog, Unsaved, Merge, "~Open project");
    DefineAction(ShowSaveDialog, Unsaved, Merge, "~Save project as...");

    DefineAction(Undo, Unsaved, NoMerge, "");
    DefineAction(Redo, Unsaved, NoMerge, "");
    DefineAction(SetHistoryIndex, Unsaved, NoMerge, "", u32 index;);
    DefineAction(Open, Unsaved, NoMerge, "", fs::path file_path;);
    DefineAction(OpenEmpty, Unsaved, NoMerge, "~New project");
    DefineAction(OpenDefault, Unsaved, NoMerge, "");
    DefineAction(Save, Unsaved, NoMerge, "", fs::path file_path;);
    DefineAction(SaveDefault, Unsaved, NoMerge, "");
    DefineAction(SaveCurrent, Unsaved, NoMerge, "~Save project");

    using Any = ActionVariant<
        Undo, Redo, SetHistoryIndex,
        Open, OpenEmpty, OpenDefault, Save, SaveDefault, SaveCurrent, ShowOpenDialog, ShowSaveDialog>;
);
