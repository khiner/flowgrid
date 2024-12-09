#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, Graph,
    DefineAction(ShowSaveSvgDialog, Saved, Merge, "~Export SVG");
    Json(ShowSaveSvgDialog);

    DefineAction(SaveSvgFile, Unsaved, NoMerge, "", ID dsp_id; fs::path dir_path;);

    using Any = ActionVariant<ShowSaveSvgDialog, SaveSvgFile>;
);
