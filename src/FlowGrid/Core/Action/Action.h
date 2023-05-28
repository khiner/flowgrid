#pragma once

#include "Actionable.h"
#include "Core/Store/StoreTypesJson.h"

using std::string;

namespace Action {
using namespace Actionable;

Define(Undo, 0, 1, NoMerge, "@cmd+z");
Define(Redo, 0, 1, NoMerge, "@shift+cmd+z");
Define(SetHistoryIndex, 0, 0, NoMerge, "", int index;);
Define(OpenProject, 0, 0, NoMerge, "", string path;);
Define(OpenEmptyProject, 0, 0, NoMerge, "~New project@cmd+n");
Define(OpenDefaultProject, 0, 1, NoMerge, "@shift+cmd+o");
Define(SaveProject, 0, 1, NoMerge, "", string path;);
Define(SaveDefaultProject, 0, 1, NoMerge, "");
Define(SaveCurrentProject, 0, 1, NoMerge, "~Save project@cmd+s");
Define(ShowOpenProjectDialog, 1, 0, Merge, "~Open project@cmd+o");
Define(ShowSaveProjectDialog, 1, 1, Merge, "~Save project as...@shift+cmd+s");
Define(CloseApplication, 1, 0, Merge, "");
Define(ToggleValue, 1, 0, NoMerge, "", StorePath path;);
Define(SetValue, 1, 0, CustomMerge, "", StorePath path; Primitive value;);
Define(SetValues, 1, 0, CustomMerge, "", StoreEntries values;);
Define(SetVector, 1, 0, CustomMerge, "", StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, 1, 0, CustomMerge, "", StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ApplyPatch, 1, 0, CustomMerge, "", Patch patch;);
Define(SetImGuiColorStyle, 1, 0, Merge, "", int id;);
Define(SetImPlotColorStyle, 1, 0, Merge, "", int id;);
Define(SetFlowGridColorStyle, 1, 0, Merge, "", int id;);
Define(SetGraphColorStyle, 1, 0, Merge, "", int id;);
Define(SetGraphLayoutStyle, 1, 0, Merge, "", int id;);
Define(ShowOpenFaustFileDialog, 1, 0, Merge, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 1, 0, Merge, "~Save DSP as...");
Define(ShowSaveFaustSvgFileDialog, 1, 0, Merge, "~Export SVG");
Define(SaveFaustFile, 0, 0, NoMerge, "", string path;);
Define(OpenFaustFile, 1, 0, CustomMerge, "", string path;);
Define(SaveFaustSvgFile, 0, 0, NoMerge, "", string path;);
Define(OpenFileDialog, 1, 1, Merge, "", string dialog_json;);
Define(CloseFileDialog, 1, 1, Merge, "");

// Define json converters for stateful actions (ones that can be saved to a project)
// todo should be done for all actions that are `Saveable`.
Json(ShowOpenProjectDialog);
Json(CloseFileDialog);
Json(ShowSaveProjectDialog);
Json(CloseApplication);
Json(ShowOpenFaustFileDialog);
Json(ShowSaveFaustFileDialog);
Json(ShowSaveFaustSvgFileDialog);
Json(OpenFileDialog, dialog_json);
Json(SetValue, path, value);
Json(SetValues, values);
Json(SetVector, path, value);
Json(SetMatrix, path, data, row_count);
Json(ToggleValue, path);
Json(ApplyPatch, patch);
Json(SetImGuiColorStyle, id);
Json(SetImPlotColorStyle, id);
Json(SetFlowGridColorStyle, id);
Json(SetGraphColorStyle, id);
Json(SetGraphLayoutStyle, id);
Json(OpenFaustFile, path);

// All actions.
// todo construct programatically during `Define` calls.
using Any = ActionVariant<
    Undo,
    Redo,
    SetHistoryIndex,
    OpenProject,
    OpenEmptyProject,
    OpenDefaultProject,
    SaveProject,
    SaveDefaultProject,
    SaveCurrentProject,
    ShowOpenProjectDialog,
    ShowSaveProjectDialog,
    CloseApplication,
    ToggleValue,
    SetValue,
    SetValues,
    SetVector,
    SetMatrix,
    ApplyPatch,
    SetImGuiColorStyle,
    SetImPlotColorStyle,
    SetFlowGridColorStyle,
    SetGraphColorStyle,
    SetGraphLayoutStyle,
    ShowOpenFaustFileDialog,
    ShowSaveFaustFileDialog,
    ShowSaveFaustSvgFileDialog,
    SaveFaustFile,
    OpenFaustFile,
    SaveFaustSvgFile,
    OpenFileDialog,
    CloseFileDialog>;

using StatefulAction = Actionable::Filter<Actionable::IsSavable, Any>::type;
using NonStatefulAction = Actionable::Filter<Actionable::NonSavable, Any>::type;

// Domain actions (todo move to their respective domain files).
using StoreAction = ActionVariant<SetValue, SetValues, SetVector, SetMatrix, ToggleValue, ApplyPatch>;
using FileDialogAction = ActionVariant<OpenFileDialog, CloseFileDialog>;
using StyleAction = ActionVariant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle, SetGraphColorStyle, SetGraphLayoutStyle>;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::StatefulAction);
} // namespace nlohmann
