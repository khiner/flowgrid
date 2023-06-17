#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Project,
    DefineAction(ShowOpenDialog, Merge, "~Open project@cmd+o");
    DefineAction(ShowSaveDialog, Merge, "~Save project as...@shift+cmd+s");

    Json(ShowOpenDialog);
    Json(ShowSaveDialog);

    DefineUnsavedAction(Undo, NoMerge, "@cmd+z");
    DefineUnsavedAction(Redo, NoMerge, "@shift+cmd+z");
    DefineUnsavedAction(SetHistoryIndex, NoMerge, "", int index;);
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
