#pragma once

#include "../Store/StoreTypesJson.h"
#include "Actionable.h"

using std::string;

namespace Action {
using namespace Actionable;

Define(Undo, 1, "@cmd+z");
Define(Redo, 1, "@shift+cmd+z");
Define(SetHistoryIndex, 0, "", int index;);
Define(OpenProject, 0, "", string path;);
Define(OpenEmptyProject, 0, "~New project@cmd+n");
Define(OpenDefaultProject, 1, "@shift+cmd+o");
Define(ShowOpenProjectDialog, 0, "~Open project@cmd+o");
Define(SaveProject, 1, "", string path;);
Define(SaveCurrentProject, 1, "~Save project@cmd+s");
Define(SaveDefaultProject, 1, "");
Define(ShowSaveProjectDialog, 1, "~Save project as...@shift+cmd+s");
Define(CloseApplication, 0, "");
Define(SetValue, 0, "", StorePath path; Primitive value;);
Define(SetValues, 0, "", StoreEntries values;);
Define(SetVector, 0, "", StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, 0, "", StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ToggleValue, 0, "", StorePath path;);
Define(ApplyPatch, 0, "", Patch patch;);
Define(SetImGuiColorStyle, 0, "", int id;);
Define(SetImPlotColorStyle, 0, "", int id;);
Define(SetFlowGridColorStyle, 0, "", int id;);
Define(SetGraphColorStyle, 0, "", int id;);
Define(SetGraphLayoutStyle, 0, "", int id;);
Define(ShowOpenFaustFileDialog, 0, "~Open DSP file");
Define(ShowSaveFaustFileDialog, 0, "~Save DSP as...");
Define(ShowSaveFaustSvgFileDialog, 0, "~Export SVG");
Define(SaveFaustFile, 0, "", string path;);
Define(OpenFaustFile, 0, "", string path;);
Define(SaveFaustSvgFile, 0, "", string path;);
Define(OpenFileDialog, 1, "", string dialog_json;);
Define(CloseFileDialog, 1, "");

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
