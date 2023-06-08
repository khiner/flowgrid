#pragma once

#include "FaustGraphAction.h"

namespace Action {
Define(ShowOpenFaustFileDialog, Merge, "~Open DSP file");
Define(ShowSaveFaustFileDialog, Merge, "~Save DSP as...");
Define(OpenFaustFile, CustomMerge, "", std::string path;);

Json(OpenFaustFile, path);
Json(ShowOpenFaustFileDialog);
Json(ShowSaveFaustFileDialog);

DefineUnsaved(SaveFaustFile, NoMerge, "", std::string path;);

using FaustFile = ActionVariant<ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, SaveFaustFile, OpenFaustFile>;
using Faust = Combine<FaustFile, FaustGraph>::type;
} // namespace Action
