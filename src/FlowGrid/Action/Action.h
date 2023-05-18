// next up:
// - improve action IDs
//   - actions all get a path in addition to their name (start with all at root, but will be heirarchical soon)
//   - `ID` type generated from path, like `StateMember`:
//     `Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0))`
// - Add `Help` string (also like StateMember)
// Move all action declaration & `Apply` handling to domain files.
#pragma once

#include "../Helper/String.h"
#include "../Helper/Variant.h"
#include "../Store/StoreTypesJson.h"

/**
An `Action` is an immutable representation of a user interaction event.
Each action stores all information needed to apply the action to a `Store` instance.
An `ActionMoment` contains an `Action` and the `TimePoint` at which the action happened.

An `Action` is a `std::variant`, which can hold any type, and thus must be large enough to hold its largest type.
- For actions holding very large structured data, using a JSON string is a good approach to keep the `Action` size down
  (at the expense of losing type safety and storing the string contents in heap memory).
- Note that adding static members does not increase the size of the parent `Action` variant.
  (You can verify this by looking at the 'Action variant size' in the Metrics->FlowGrid window.)
*/
#define DefineAction(ActionName, ...)                                                          \
    struct ActionName {                                                                        \
        inline const static std::string Name{StringHelper::PascalToSentenceCase(#ActionName)}; \
        __VA_ARGS__;                                                                           \
    };

namespace action {
template<typename T>
concept Actionable = requires() {
    { T::Name } -> std::same_as<const std::string &>;
    // { T::Id } -> std::same_as<const std::string>;
};

// E.g. `action::GetName<MyAction>()`
template<Actionable T> std::string GetName() { return T::Name; }

// template<Actionable T> std::string GetId() { return T::Id; }

DefineAction(Undo);
DefineAction(Redo);
DefineAction(SetHistoryIndex, int index;);
DefineAction(OpenProject, std::string path;);
DefineAction(OpenEmptyProject);
DefineAction(OpenDefaultProject);
DefineAction(ShowOpenProjectDialog);
DefineAction(SaveProject, std::string path;);
DefineAction(SaveCurrentProject);
DefineAction(SaveDefaultProject);
DefineAction(ShowSaveProjectDialog);
DefineAction(CloseApplication);
DefineAction(SetValue, StorePath path; Primitive value;);
DefineAction(SetValues, StoreEntries values;);
DefineAction(SetVector, StorePath path; std::vector<Primitive> value;);
DefineAction(SetMatrix, StorePath path; std::vector<Primitive> data; Count row_count;);
DefineAction(ToggleValue, StorePath path;);
DefineAction(ApplyPatch, Patch patch;);
DefineAction(SetImGuiColorStyle, int id;);
DefineAction(SetImPlotColorStyle, int id;);
DefineAction(SetFlowGridColorStyle, int id;);
DefineAction(SetGraphColorStyle, int id;);
DefineAction(SetGraphLayoutStyle, int id;);
DefineAction(ShowOpenFaustFileDialog);
DefineAction(ShowSaveFaustFileDialog);
DefineAction(ShowSaveFaustSvgFileDialog);
DefineAction(SaveFaustFile, std::string path;);
DefineAction(OpenFaustFile, std::string path;);
DefineAction(SaveFaustSvgFile, std::string path;);
DefineAction(OpenFileDialog, std::string dialog_json;);
DefineAction(CloseFileDialog);

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
using StatefulAction = Combine<StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// All actions.
using Action = Combine<ProjectAction, StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// Composite action types.
using ActionMoment = std::pair<Action, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;

// Default-construct an action by its variant index (which is also its `ID`).
// Adapted from: https://stackoverflow.com/a/60567091/780425
template<ID I = 0> Action Create(ID index) {
    if constexpr (I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? Action{std::in_place_index<I>} : Create<I + 1>(index - 1);
}

#include "../../Boost/mp11/mp_find.h"

// E.g. `ID action_id = id<action_type>`
// An action's ID is its index in the `Action` variant.
// Down the road, this means `Action` would need to be append-only (no order changes) for backwards compatibility.
// Not worried about that right now, since it should be easy enough to replace with some UUID system later.
// Index is simplest.
// Mp11 approach from: https://stackoverflow.com/a/66386518/780425
template<typename T> constexpr ID id = mp_find<Action, T>::value;

inline static const std::unordered_map<ID, string> ShortcutForId = {
    {id<Undo>, "cmd+z"},
    {id<Redo>, "shift+cmd+z"},
    {id<OpenEmptyProject>, "cmd+n"},
    {id<ShowOpenProjectDialog>, "cmd+o"},
    {id<OpenDefaultProject>, "shift+cmd+o"},
    {id<SaveCurrentProject>, "cmd+s"},
    {id<ShowSaveProjectDialog>, "shift+cmd+s"},
};

constexpr ID GetId(const Action &action) { return action.index(); }
constexpr ID GetId(const StatefulAction &action) { return action.index(); }

string GetName(const ProjectAction &action);
string GetName(const StatefulAction &action);
string GetShortcut(const Action &);
string GetMenuLabel(const Action &);
Gesture MergeGesture(const Gesture &);
} // namespace action

/**
 This is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
 Defined in `Project.cpp`.
*/
bool q(action::Action &&a, bool flush = false);

// These are also defined in `Project.cpp`.
bool ActionAllowed(ID);
bool ActionAllowed(const action::Action &);

namespace nlohmann {
DeclareJson(action::StatefulAction);
} // namespace nlohmann
