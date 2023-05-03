#pragma once

#include "App.h"

#include "nlohmann/json.hpp"

namespace nlohmann {
// Convert `std::chrono::time_point`s to/from JSON.
// Based on https://github.com/nlohmann/json/issues/2159#issuecomment-638104529
template<typename Clock, typename Duration> struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static void to_json(json &, const std::chrono::time_point<Clock, Duration> &);
    static void from_json(const json &, std::chrono::time_point<Clock, Duration> &);
};

void to_json(json &, const Primitive &);
void from_json(const json &, Primitive &);
} // namespace nlohmann

#define DeclareJsonType(Type)                     \
    void to_json(nlohmann::json &, const Type &); \
    void from_json(const nlohmann::json &, Type &);

DeclareJsonType(StatePath);

namespace nlohmann {
DeclareJsonType(StateAction);
DeclareJsonType(ProjectAction);
} // namespace nlohmann

DeclareJsonType(PatchOp);
DeclareJsonType(Patch);
DeclareJsonType(StatePatch);

DeclareJsonType(FileDialogData);

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
