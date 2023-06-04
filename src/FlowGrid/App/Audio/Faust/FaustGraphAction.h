#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace Action {
Define(SetGraphColorStyle, 1, 0, Merge, "", int id;);
Define(SetGraphLayoutStyle, 1, 0, Merge, "", int id;);
Define(ShowSaveFaustSvgFileDialog, 1, 0, Merge, "~Export SVG");
Define(SaveFaustSvgFile, 0, 0, NoMerge, "", std::string path;);

Json(SetGraphColorStyle, id);
Json(SetGraphLayoutStyle, id);
Json(ShowSaveFaustSvgFileDialog);

using FaustGraphAction = ActionVariant<SetGraphColorStyle, SetGraphLayoutStyle, ShowSaveFaustSvgFileDialog, SaveFaustSvgFile>;
} // namespace Action
