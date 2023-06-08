#pragma once

#include "FaustGraphAction.h"

DefineActionType(
    FaustFile,
    DefineAction(ShowOpenDialog, Merge, "~Open DSP file");
    DefineAction(ShowSaveDialog, Merge, "~Save DSP as...");
    DefineAction(Open, CustomMerge, "", std::string path;);

    Json(Open, path);
    Json(ShowOpenDialog);
    Json(ShowSaveDialog);

    DefineActionUnsaved(Save, NoMerge, "", std::string path;);

    using Any = ActionVariant<ShowOpenDialog, ShowSaveDialog, Save, Open>;
);

namespace Action {
using Faust = Combine<FaustFile::Any, FaustGraph::Any>::type;
} // namespace Action
