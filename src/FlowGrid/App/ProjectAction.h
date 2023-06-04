#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace Action {
Define(Undo, 0, 1, NoMerge, "@cmd+z");
Define(Redo, 0, 1, NoMerge, "@shift+cmd+z");
Define(SetHistoryIndex, 0, 0, NoMerge, "", int index;);
Define(OpenProject, 0, 0, NoMerge, "", std::string path;);
Define(OpenEmptyProject, 0, 0, NoMerge, "~New project@cmd+n");
Define(OpenDefaultProject, 0, 1, NoMerge, "@shift+cmd+o");
Define(SaveProject, 0, 1, NoMerge, "", std::string path;);
Define(SaveDefaultProject, 0, 1, NoMerge, "");
Define(SaveCurrentProject, 0, 1, NoMerge, "~Save project@cmd+s");
Define(ShowOpenProjectDialog, 1, 0, Merge, "~Open project@cmd+o");
Define(ShowSaveProjectDialog, 1, 1, Merge, "~Save project as...@shift+cmd+s");

// Define json converters for stateful actions (ones that can be saved to a project)
// todo should be done for all actions that are `Saveable`.
Json(ShowOpenProjectDialog);
Json(ShowSaveProjectDialog);

using ProjectAction = ActionVariant<
    Undo, Redo, SetHistoryIndex,
    OpenProject, OpenEmptyProject, OpenDefaultProject, SaveProject, SaveDefaultProject, SaveCurrentProject, ShowOpenProjectDialog, ShowSaveProjectDialog>;
} // namespace Action
