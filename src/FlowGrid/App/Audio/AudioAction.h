#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Json.h"

namespace Action {
using namespace Actionable;
Define(SetGraphColorStyle, 1, 0, Merge, "", int id;);
Define(SetGraphLayoutStyle, 1, 0, Merge, "", int id;);
Define(ShowOpenFaustFileDialog, 1, 0, Merge, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 1, 0, Merge, "~Save DSP as...");
Define(ShowSaveFaustSvgFileDialog, 1, 0, Merge, "~Export SVG");
Define(SaveFaustFile, 0, 0, NoMerge, "", std::string path;);
Define(OpenFaustFile, 1, 0, CustomMerge, "", std::string path;);
Define(SaveFaustSvgFile, 0, 0, NoMerge, "", std::string path;);

Json(SetGraphColorStyle, id);
Json(SetGraphLayoutStyle, id);
Json(OpenFaustFile, path);
Json(ShowOpenFaustFileDialog);
Json(ShowSaveFaustFileDialog);
Json(ShowSaveFaustSvgFileDialog);

using AudioAction = ActionVariant<
    SetGraphColorStyle, SetGraphLayoutStyle,
    ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, ShowSaveFaustSvgFileDialog,
    SaveFaustFile, OpenFaustFile, SaveFaustSvgFile>;
} // namespace Action
