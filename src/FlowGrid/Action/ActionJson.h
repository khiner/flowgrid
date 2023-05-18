#pragma once

#include "Action.h"

#include "nlohmann/json.hpp"

#define DeclareJsonType(Type)                     \
    void to_json(nlohmann::json &, const Type &); \
    void from_json(const nlohmann::json &, Type &);

namespace nlohmann {
template<typename Clock, typename Duration> struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static void to_json(json &, const std::chrono::time_point<Clock, Duration> &);
    static void from_json(const json &, std::chrono::time_point<Clock, Duration> &);
};
} // namespace nlohmann

namespace nlohmann {
DeclareJsonType(StorePath);
DeclareJsonType(PatchOp);
DeclareJsonType(Patch);
DeclareJsonType(StatePatch);
} // namespace nlohmann

namespace Actions {
DeclareJsonType(Undo);
DeclareJsonType(Redo);
DeclareJsonType(OpenEmptyProject);
DeclareJsonType(OpenDefaultProject);
DeclareJsonType(ShowOpenProjectDialog);
DeclareJsonType(CloseFileDialog);
DeclareJsonType(SaveCurrentProject);
DeclareJsonType(SaveDefaultProject);
DeclareJsonType(ShowSaveProjectDialog);
DeclareJsonType(CloseApplication);
DeclareJsonType(ShowOpenFaustFileDialog);
DeclareJsonType(ShowSaveFaustFileDialog);
DeclareJsonType(ShowSaveFaustSvgFileDialog);

DeclareJsonType(SetHistoryIndex);
DeclareJsonType(OpenProject);
DeclareJsonType(OpenFileDialog);
DeclareJsonType(SaveProject);
DeclareJsonType(SetValue);
DeclareJsonType(SetValues);
DeclareJsonType(SetVector);
DeclareJsonType(SetMatrix);
DeclareJsonType(ToggleValue);
DeclareJsonType(ApplyPatch);
DeclareJsonType(SetImGuiColorStyle);
DeclareJsonType(SetImPlotColorStyle);
DeclareJsonType(SetFlowGridColorStyle);
DeclareJsonType(SetGraphColorStyle);
DeclareJsonType(SetGraphLayoutStyle);
DeclareJsonType(SaveFaustFile);
DeclareJsonType(OpenFaustFile);
DeclareJsonType(SaveFaustSvgFile);
} // namespace Actions

namespace nlohmann {
DeclareJsonType(action::StatefulAction);
} // namespace nlohmann
