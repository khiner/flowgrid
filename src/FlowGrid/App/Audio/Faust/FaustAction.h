#pragma once

#include "FaustGraphAction.h"

DefineActionType(
    FaustFile,
    DefineAction(ShowOpenDialog, Merge, "~Open DSP file");
    DefineAction(ShowSaveDialog, Merge, "~Save DSP as...");
    DefineAction(Open, CustomMerge, "", fs::path file_path;);

    Json(Open, file_path);
    Json(ShowOpenDialog);
    Json(ShowSaveDialog);

    DefineUnsavedAction(Save, NoMerge, "", fs::path file_path;);

    using Any = ActionVariant<ShowOpenDialog, ShowSaveDialog, Save, Open>;
);

namespace Action {
using Faust = Combine<FaustFile::Any, FaustGraph::Any>::type;
} // namespace Action
