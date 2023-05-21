#pragma once

#include "../Store/StoreTypesJson.h"
#include "Actionable.h"

using std::string;

namespace Action {
DefineContextual(Undo);
DefineContextual(Redo);
Define(SetHistoryIndex, int index;);
Define(OpenProject, string path;);
Define(OpenEmptyProject);
DefineContextual(OpenDefaultProject);
Define(ShowOpenProjectDialog);
DefineContextual(SaveProject, string path;);
DefineContextual(SaveCurrentProject);
DefineContextual(SaveDefaultProject);
DefineContextual(ShowSaveProjectDialog);
Define(CloseApplication);
Define(SetValue, StorePath path; Primitive value;);
Define(SetValues, StoreEntries values;);
Define(SetVector, StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ToggleValue, StorePath path;);
Define(ApplyPatch, Patch patch;);
Define(SetImGuiColorStyle, int id;);
Define(SetImPlotColorStyle, int id;);
Define(SetFlowGridColorStyle, int id;);
Define(SetGraphColorStyle, int id;);
Define(SetGraphLayoutStyle, int id;);
Define(ShowOpenFaustFileDialog);
Define(ShowSaveFaustFileDialog);
Define(ShowSaveFaustSvgFileDialog);
Define(SaveFaustFile, string path;);
Define(OpenFaustFile, string path;);
Define(SaveFaustSvgFile, string path;);
DefineContextual(OpenFileDialog, string dialog_json;);
DefineContextual(CloseFileDialog);

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

using ProjectAction = std::variant<
    Undo, Redo, SetHistoryIndex,
    OpenProject, OpenEmptyProject, OpenDefaultProject,
    SaveProject, SaveDefaultProject, SaveCurrentProject, SaveFaustFile, SaveFaustSvgFile>;

// Actions that apply directly to the store.
using StoreAction = std::variant<SetValue, SetValues, SetVector, SetMatrix, ToggleValue, ApplyPatch>;

// Domain actions (todo move to their respective domain files).
using FileDialogAction = std::variant<OpenFileDialog, CloseFileDialog>;
using StyleAction = std::variant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle, SetGraphColorStyle, SetGraphLayoutStyle>;

using OtherAction = std::variant<
    ShowOpenProjectDialog, ShowSaveProjectDialog, ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, ShowSaveFaustSvgFileDialog, OpenFaustFile,
    CloseApplication>;

// Actions that update state (as opposed to actions that only have non-state-updating side effects, like saving a file).
// These get added to the gesture history, and are saved in a `.fga` (FlowGridAction) project.
using StatefulAction = Variant::Combine<StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// All actions.
using Any = Variant::Combine<ProjectAction, StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;

// Create map of stateful action names to indices.
// Force initialization to happen before `main()` is called.
inline std::unordered_map<string, size_t> StatefulNameToIndex;
inline const bool MapInitialized = [] {
    CreateNameToIndexMap<StatefulAction>::Init(StatefulNameToIndex);
    return true;
}();

// Usage: `ID action_id = action::id<ActionType>`
// An action's ID is its index in the `StatefulAction` variant.
// Note that action JSON serialization is keyed by the action _name_, not its index/ID,
// and thus action-formatted projects are still valid regardless of the declaration order of actions within the `StatefulAction` struct.
template<typename T> constexpr ID id = Variant::Index<T, Any>::value;

inline static const std::unordered_map<ID, string> ShortcutForId = {
    {id<Undo>, "cmd+z"},
    {id<Redo>, "shift+cmd+z"},
    {id<OpenEmptyProject>, "cmd+n"},
    {id<ShowOpenProjectDialog>, "cmd+o"},
    {id<OpenDefaultProject>, "shift+cmd+o"},
    {id<SaveCurrentProject>, "cmd+s"},
    {id<ShowSaveProjectDialog>, "shift+cmd+s"},
};

constexpr ID GetId(const Any &action) { return action.index(); }
constexpr ID GetId(const StatefulAction &action) { return action.index(); }

bool IsAllowed(const Any &);
string GetName(const StatefulAction &);
string GetShortcut(const Any &);
string GetMenuLabel(const Any &);
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
