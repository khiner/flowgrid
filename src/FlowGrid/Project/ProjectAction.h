#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Project,
    DefineUnsavedAction(ShowOpenDialog, Merge, "~Open project@cmd+o");
    DefineUnsavedAction(ShowSaveDialog, Merge, "~Save project as...@shift+cmd+s");

    DefineUnsavedAction(Undo, NoMerge, "@cmd+z");
    DefineUnsavedAction(Redo, NoMerge, "@shift+cmd+z");
    DefineUnsavedAction(SetHistoryIndex, NoMerge, "", u32 index;);
    DefineUnsavedAction(Open, NoMerge, "", fs::path file_path;);
    DefineUnsavedAction(OpenEmpty, NoMerge, "~New project@cmd+n");
    DefineUnsavedAction(OpenDefault, NoMerge, "@shift+cmd+o");
    DefineUnsavedAction(Save, NoMerge, "", fs::path file_path;);
    DefineUnsavedAction(SaveDefault, NoMerge, "");
    DefineUnsavedAction(SaveCurrent, NoMerge, "~Save project@cmd+s");

    using Any = ActionVariant<
        Undo, Redo, SetHistoryIndex,
        Open, OpenEmpty, OpenDefault, Save, SaveDefault, SaveCurrent, ShowOpenDialog, ShowSaveDialog>;
);
