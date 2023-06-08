#pragma once

#include "Core/Action/Action.h"

namespace Action {
Define(SetGraphColorStyle, Merge, "", int id;);
Define(SetGraphLayoutStyle, Merge, "", int id;);
Define(ShowSaveFaustSvgFileDialog, Merge, "~Export SVG");

Json(SetGraphColorStyle, id);
Json(SetGraphLayoutStyle, id);
Json(ShowSaveFaustSvgFileDialog);

DefineUnsaved(SaveFaustSvgFile, NoMerge, "", std::string path;);

using FaustGraph = ActionVariant<SetGraphColorStyle, SetGraphLayoutStyle, ShowSaveFaustSvgFileDialog, SaveFaustSvgFile>;
} // namespace Action
