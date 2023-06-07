#pragma once

#include "FaustGraphAction.h"

namespace Action {
Define(ShowOpenFaustFileDialog, 1, Merge, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 1, Merge, "~Save DSP as...");
Define(SaveFaustFile, 0, NoMerge, "", std::string path;);
Define(OpenFaustFile, 1, CustomMerge, "", std::string path;);

Json(OpenFaustFile, path);
Json(ShowOpenFaustFileDialog);
Json(ShowSaveFaustFileDialog);

using FaustFile = ActionVariant<ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, SaveFaustFile, OpenFaustFile>;
using Faust = Combine<FaustFile, FaustGraph>::type;
} // namespace Action
