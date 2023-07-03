#pragma once

#include "FaustGraphAction.h"

DefineActionType(
    FaustFile,
    DefineUnsavedAction(ShowOpenDialog, Merge, "~Open DSP file");
    DefineUnsavedAction(ShowSaveDialog, Merge, "~Save DSP as...");
    DefineAction(Open, CustomMerge, "", fs::path file_path;);

    Json(Open, file_path);

    DefineUnsavedAction(Save, NoMerge, "", fs::path file_path;);

    using Any = ActionVariant<ShowOpenDialog, ShowSaveDialog, Save, Open>;
);

namespace Action {
using Faust = Combine<FaustFile::Any, FaustGraph::Any>;
} // namespace Action
