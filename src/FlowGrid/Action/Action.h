#pragma once

#include "../Store/StoreTypes.h"

using std::vector;

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

// Utility to make a variant visitor out of lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
template<class... Ts> struct visitor : Ts... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

// E.g. Match(action, [](const ProjectAction &a) { ... }, [](const StatefulAction &a) { ... });
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

// Utility to flatten two variants together into one variant.
// Based on https://stackoverflow.com/a/59251342/780425, but adds support for > 2 variants using template recursion.
// E.g. Combine<Variant1, Variant2, Variant3>
template<typename... Vars>
struct Combine;

template<typename Var1>
struct Combine<Var1> {
    using type = Var1;
};

template<typename... Ts1, typename... Ts2, typename... Vars>
struct Combine<std::variant<Ts1...>, std::variant<Ts2...>, Vars...> {
    using type = typename Combine<std::variant<Ts1..., Ts2...>, Vars...>::type;
};

namespace Actions {
struct Undo {};
struct Redo {};
struct SetHistoryIndex {
    int index;
};

struct OpenProject {
    string path;
};
struct OpenEmptyProject {};
struct OpenDefaultProject {};

struct ShowOpenProjectDialog {};

struct SaveProject {
    string path;
};
struct SaveCurrentProject {};
struct SaveDefaultProject {};
struct ShowSaveProjectDialog {};

struct CloseApplication {};

struct SetValue {
    StorePath path;
    Primitive value;
};
struct SetValues {
    StoreEntries values;
};
struct SetVector {
    StorePath path;
    vector<Primitive> value;
};
struct SetMatrix {
    StorePath path;
    vector<Primitive> data;
    Count row_count; // Column count derived from `data.size() / row_count`.
};
struct ToggleValue {
    StorePath path;
};
struct ApplyPatch {
    Patch patch;
};

struct SetImGuiColorStyle {
    int id;
};
struct SetImPlotColorStyle {
    int id;
};
struct SetFlowGridColorStyle {
    int id;
};
struct SetGraphColorStyle {
    int id;
};
struct SetGraphLayoutStyle {
    int id;
};

struct ShowOpenFaustFileDialog {};
struct ShowSaveFaustFileDialog {};
struct ShowSaveFaustSvgFileDialog {};
struct SaveFaustFile {
    string path;
};
struct OpenFaustFile {
    string path;
};
struct SaveFaustSvgFile {
    string path;
};

struct OpenFileDialog {
    string dialog_json;
}; // Storing as JSON string instead of the raw struct to reduce variant size. (Raw struct is 120 bytes.)
struct CloseFileDialog {};
} // namespace Actions

using namespace Actions;

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

// All actions.
using Action = Combine<ProjectAction, StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// Actions that update state (as opposed to actions that only have non-state-updating side effects, like saving a file).
// These get added to the gesture history, and are saved in a `.fga` (FlowGridAction) project.
using StatefulAction = Combine<StoreAction, FileDialogAction, StyleAction, OtherAction>::type;

// All actions that don't have any member data.
using EmptyAction = std::variant<
    Undo,
    Redo,
    OpenEmptyProject,
    OpenDefaultProject,
    ShowOpenProjectDialog,
    CloseFileDialog,
    SaveCurrentProject,
    SaveDefaultProject,
    ShowSaveProjectDialog,
    CloseApplication,
    ShowOpenFaustFileDialog,
    ShowSaveFaustFileDialog,
    ShowSaveFaustSvgFileDialog>;

namespace action {

using ActionMoment = std::pair<Action, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = vector<StatefulActionMoment>;
using Gestures = vector<Gesture>;

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

// Note: ID here is index within `Action` variant, not the `EmptyAction` variant.
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
string GetShortcut(const EmptyAction &);
string GetMenuLabel(const EmptyAction &);
Gesture MergeGesture(const Gesture &);
} // namespace action

/**
 This is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
 Defined in `Project.cpp`.
*/
bool q(Action &&a, bool flush = false);

// These are also defined in `Project.cpp`.
bool ActionAllowed(ID);
bool ActionAllowed(const Action &);
bool ActionAllowed(const EmptyAction &);
