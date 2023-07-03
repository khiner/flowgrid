#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FaustGraph,
    DefineAction(SetColorStyle, Merge, "", int id;);
    DefineAction(SetLayoutStyle, Merge, "", int id;);
    DefineAction(ShowSaveSvgDialog, Merge, "~Export SVG");

    Json(SetColorStyle, id);
    Json(SetLayoutStyle, id);
    Json(ShowSaveSvgDialog);

    DefineUnsavedAction(SaveSvgFile, NoMerge, "", fs::path dir_path;);

    using Any = ActionVariant<SetColorStyle, SetLayoutStyle, ShowSaveSvgDialog, SaveSvgFile>;
);
