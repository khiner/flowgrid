#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace Action {
Define(ShowOpenProjectDialog, Merge, "~Open project@cmd+o");
Define(ShowSaveProjectDialog, Merge, "~Save project as...@shift+cmd+s");

Json(ShowOpenProjectDialog);
Json(ShowSaveProjectDialog);

DefineUnsaved(Undo, NoMerge, "@cmd+z");
DefineUnsaved(Redo, NoMerge, "@shift+cmd+z");
DefineUnsaved(SetHistoryIndex, NoMerge, "", int index;);
DefineUnsaved(OpenProject, NoMerge, "", std::string path;);
DefineUnsaved(OpenEmptyProject, NoMerge, "~New project@cmd+n");
DefineUnsaved(OpenDefaultProject, NoMerge, "@shift+cmd+o");
DefineUnsaved(SaveProject, NoMerge, "", std::string path;);
DefineUnsaved(SaveDefaultProject, NoMerge, "");
DefineUnsaved(SaveCurrentProject, NoMerge, "~Save project@cmd+s");

using Project = ActionVariant<
    Undo, Redo, SetHistoryIndex,
    OpenProject, OpenEmptyProject, OpenDefaultProject, SaveProject, SaveDefaultProject, SaveCurrentProject, ShowOpenProjectDialog, ShowSaveProjectDialog>;
} // namespace Action
