#pragma once

#include "../Store/StoreTypesJson.h"
#include "Actionable.h"

using std::string;

namespace Action {
using namespace Actionable;

Define(Undo, 1, None, "@cmd+z");
Define(Redo, 1, None, "@shift+cmd+z");
Define(SetHistoryIndex, 0, None, "", int index;);
Define(OpenProject, 0, None, "", string path;);
Define(OpenEmptyProject, 0, None, "~New project@cmd+n");
Define(OpenDefaultProject, 1, None, "@shift+cmd+o");
Define(SaveProject, 1, None, "", string path;);
Define(SaveDefaultProject, 1, None, "");
Define(SaveCurrentProject, 1, None, "~Save project@cmd+s");
Define(ShowOpenProjectDialog, 0, Same, "~Open project@cmd+o");
Define(ShowSaveProjectDialog, 1, Same, "~Save project as...@shift+cmd+s");
Define(CloseApplication, 0, Same, "");
Define(ToggleValue, 0, None, "", StorePath path;);
Define(SetValue, 0, Custom, "", StorePath path; Primitive value;);
Define(SetValues, 0, Custom, "", StoreEntries values;);
Define(SetVector, 0, Custom, "", StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, 0, Custom, "", StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ApplyPatch, 0, Custom, "", Patch patch;);
Define(SetImGuiColorStyle, 0, Same, "", int id;);
Define(SetImPlotColorStyle, 0, Same, "", int id;);
Define(SetFlowGridColorStyle, 0, Same, "", int id;);
Define(SetGraphColorStyle, 0, Same, "", int id;);
Define(SetGraphLayoutStyle, 0, Same, "", int id;);
Define(ShowOpenFaustFileDialog, 0, Same, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 0, Same, "~Save DSP as...");
Define(ShowSaveFaustSvgFileDialog, 0, Same, "~Export SVG");
Define(SaveFaustFile, 0, None, "", string path;);
Define(OpenFaustFile, 0, Custom, "", string path;);
Define(SaveFaustSvgFile, 0, None, "", string path;);
Define(OpenFileDialog, 1, Same, "", string dialog_json;);
Define(CloseFileDialog, 1, Same, "");

// Define json converters for stateful actions (ones that can be saved to a project)
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

using ProjectAction = ActionVariant<
    Undo, Redo, SetHistoryIndex,
    OpenProject, OpenEmptyProject, OpenDefaultProject,
    SaveProject, SaveDefaultProject, SaveCurrentProject, SaveFaustFile, SaveFaustSvgFile>;

// Actions that apply directly to the store.
using StoreAction = ActionVariant<SetValue, SetValues, SetVector, SetMatrix, ToggleValue, ApplyPatch>;

// Domain actions (todo move to their respective domain files).
using FileDialogAction = ActionVariant<OpenFileDialog, CloseFileDialog>;
using StyleAction = ActionVariant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle, SetGraphColorStyle, SetGraphLayoutStyle>;

using OtherAction = ActionVariant<
    ShowOpenProjectDialog, ShowSaveProjectDialog, ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, ShowSaveFaustSvgFileDialog, OpenFaustFile,
    CloseApplication>;

// Actions that update state (as opposed to actions that only have non-state-updating side effects, like saving a file).
// These get added to the gesture history, and are saved in a `.fga` (FlowGridAction) project.
using StatefulAction = Combine<StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// All actions.
using Any = Combine<ProjectAction, StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;

Gesture MergeGesture(const Gesture &);
} // namespace Action

/**
 This is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
 Defined in `Project.cpp`.
*/
bool q(const Action::Any &&, bool flush = false);

namespace nlohmann {
DeclareJson(Action::StatefulAction);
} // namespace nlohmann
