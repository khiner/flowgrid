#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, Graph,
    DefineAction(ShowSaveSvgDialog, Merge, "~Export SVG");
    Json(ShowSaveSvgDialog);

    DefineUnsavedAction(SaveSvgFile, NoMerge, "", fs::path dir_path;);

    using Any = ActionVariant<ShowSaveSvgDialog, SaveSvgFile>;
);
