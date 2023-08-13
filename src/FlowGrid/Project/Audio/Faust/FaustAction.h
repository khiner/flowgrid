#pragma once

#include "FaustGraphAction.h"
#include "FaustGraphStyleAction.h"

DefineNestedActionType(
    Faust, File,
    DefineUnsavedAction(ShowOpenDialog, Merge, "~Open DSP file");
    DefineUnsavedAction(ShowSaveDialog, Merge, "~Save DSP as...");
    DefineAction(Open, CustomMerge, "", fs::path file_path;);

    Json(Open, file_path);

    DefineUnsavedAction(Save, NoMerge, "", fs::path file_path;);

    using Any = ActionVariant<ShowOpenDialog, ShowSaveDialog, Save, Open>;
);

DefineActionType(
    Faust,
    using Any = Combine<Faust::File::Any, Faust::Graph::Any, Faust::GraphStyle::Any>;
);
