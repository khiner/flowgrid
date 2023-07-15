#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FaustGraph,
    DefineAction(ShowSaveSvgDialog, Merge, "~Export SVG");
    Json(ShowSaveSvgDialog);

    DefineUnsavedAction(SaveSvgFile, NoMerge, "", fs::path dir_path;);

    using Any = ActionVariant<ShowSaveSvgDialog, SaveSvgFile>;
);
