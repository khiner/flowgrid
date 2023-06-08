#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Project,
    DefineAction(ShowOpenDialog, Merge, "~Open project@cmd+o");
    DefineAction(ShowSaveDialog, Merge, "~Save project as...@shift+cmd+s");

    Json(ShowOpenDialog);
    Json(ShowSaveDialog);

    DefineActionUnsaved(Undo, NoMerge, "@cmd+z");
    DefineActionUnsaved(Redo, NoMerge, "@shift+cmd+z");
    DefineActionUnsaved(SetHistoryIndex, NoMerge, "", int index;);
    DefineActionUnsaved(Open, NoMerge, "", std::string path;);
    DefineActionUnsaved(OpenEmpty, NoMerge, "~New project@cmd+n");
    DefineActionUnsaved(OpenDefault, NoMerge, "@shift+cmd+o");
    DefineActionUnsaved(Save, NoMerge, "", std::string path;);
    DefineActionUnsaved(SaveDefault, NoMerge, "");
    DefineActionUnsaved(SaveCurrent, NoMerge, "~Save project@cmd+s");

    using Any = ActionVariant<
        Undo, Redo, SetHistoryIndex,
        Open, OpenEmpty, OpenDefault, Save, SaveDefault, SaveCurrent, ShowOpenDialog, ShowSaveDialog>;
);
