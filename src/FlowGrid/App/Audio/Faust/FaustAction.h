#pragma once

#include "FaustGraphAction.h"

namespace Action {
Define(ShowOpenFaustFileDialog, 1, 0, Merge, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 1, 0, Merge, "~Save DSP as...");
Define(SaveFaustFile, 0, 0, NoMerge, "", std::string path;);
Define(OpenFaustFile, 1, 0, CustomMerge, "", std::string path;);

Json(OpenFaustFile, path);
Json(ShowOpenFaustFileDialog);
Json(ShowSaveFaustFileDialog);

using FaustFileAction = ActionVariant<ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, SaveFaustFile, OpenFaustFile>;
using FaustAction = Combine<FaustFileAction, FaustGraphAction>::type;
} // namespace Action
