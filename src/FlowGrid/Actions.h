#pragma once

#include <map>

#include "Helper/File.h"
#include "Helper/String.h"
#include "Helper/Time.h"
#include "Primitive.h"

using std::map;

/**
An ID is used to uniquely identify something.

## Notable uses

### `StateMember`

A `StateMember` has an `ID id` instance member.
`StateMember::Id` reflects its `StatePath Path`, using `ImHashStr` to calculate its own `Id` using its parent's `Id` as a seed.
In the same way, each segment in `StateMember::Path` is calculated by appending its own `PathSegment` to its parent's `Path`.
This exactly reflects the way ImGui calculates its window/tab/dockspace/etc. ID calculation.
A drawable `UIStateMember` uses its `ID` (which is also an `ImGuiID`) as the ID for the top-level `ImGui` widget rendered during its `Draw` call.
This results in the nice property that we can find any `UIStateMember` instance by calling `StateMember::WithId.contains(ImGui::GetHoveredID())` any time during a `UIStateMember::Draw`.
 */
using ID = unsigned int;
using StatePath = fs::path;

static const StatePath RootPath{"/"};

using StoreEntry = pair<StatePath, Primitive>;
using StoreEntries = vector<StoreEntry>;

struct StatePathHash {
    auto operator()(const StatePath &p) const noexcept { return fs::hash_value(p); }
};

struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    PatchOp::Type Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};

using PatchOps = map<StatePath, PatchOp>;

static constexpr auto AddOp = PatchOp::Type::Add;
static constexpr auto RemoveOp = PatchOp::Type::Remove;
static constexpr auto ReplaceOp = PatchOp::Type::Replace;

struct Patch {
    PatchOps Ops;
    StatePath BasePath{RootPath};

    bool empty() const noexcept { return Ops.empty(); }
};

struct StatePatch {
    Patch Patch{};
    TimePoint Time{};
};

string to_string(const Primitive &);
string to_string(PatchOp::Type);

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

// E.g. Match(action, [](const ProjectAction &a) { ... }, [](const StateAction &a) { ... });
#define Match(Variant, ...) std::visit(visitor{__VA_ARGS__}, Variant);

// Utility to flatten two variants together into one variant.
// From https://stackoverflow.com/a/59251342/780425
// E.g. Combine<Variant1, Variant2>
template<typename Var1, typename Var2> struct Combine;
template<typename... Ts1, typename... Ts2> struct Combine<std::variant<Ts1...>, std::variant<Ts2...>> {
    using type = std::variant<Ts1..., Ts2...>;
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
struct OpenFileDialog {
    string dialog_json;
}; // Storing as JSON string instead of the raw struct to reduce variant size. (Raw struct is 120 bytes.)
struct CloseFileDialog {};

struct SaveProject {
    string path;
};
struct SaveCurrentProject {};
struct SaveDefaultProject {};
struct ShowSaveProjectDialog {};

struct CloseApplication {};

struct SetValue {
    StatePath path;
    Primitive value;
};
struct SetValues {
    StoreEntries values;
};
struct SetVector {
    StatePath path;
    vector<Primitive> value;
};
struct ToggleValue {
    StatePath path;
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
} // namespace Actions

using namespace Actions;

// Actions that don't directly update state.
// These don't get added to the action/gesture history, since they result in side effects that don't change values in the main state store.
// These are not saved in a FlowGridAction (.fga) project.
using ProjectAction = std::variant<
    Undo, Redo, SetHistoryIndex,
    OpenProject, OpenEmptyProject, OpenDefaultProject,
    SaveProject, SaveDefaultProject, SaveCurrentProject, SaveFaustFile, SaveFaustSvgFile>;
using StateAction = std::variant<
    OpenFileDialog, CloseFileDialog,
    ShowOpenProjectDialog, ShowSaveProjectDialog, ShowOpenFaustFileDialog, ShowSaveFaustFileDialog, ShowSaveFaustSvgFileDialog,
    OpenFaustFile,

    SetValue, SetValues, SetVector, ToggleValue, ApplyPatch,

    SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle, SetGraphColorStyle,
    SetGraphLayoutStyle,

    CloseApplication>;
using Action = Combine<ProjectAction, StateAction>::type;
using ActionID = ID;

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

using ActionMoment = pair<Action, TimePoint>;
using StateActionMoment = std::pair<StateAction, TimePoint>;
using Gesture = vector<StateActionMoment>;
using Gestures = vector<Gesture>;

// Default-construct an action by its variant index (which is also its `ID`).
// Adapted from: https://stackoverflow.com/a/60567091/780425
template<ID I = 0> Action Create(ID index) {
    if constexpr (I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? Action{std::in_place_index<I>} : Create<I + 1>(index - 1);
}

#include "../Boost/mp11/mp_find.h"

// E.g. `ActionID action_id = id<action_type>`
// An action's ID is its index in the `Action` variant.
// Down the road, this means `Action` would need to be append-only (no order changes) for backwards compatibility.
// Not worried about that right now, since it should be easy enough to replace with some UUID system later.
// Index is simplest.
// Mp11 approach from: https://stackoverflow.com/a/66386518/780425
template<typename T> constexpr ActionID id = mp_find<Action, T>::value;

#define ActionName(action_var_name) PascalToSentenceCase(#action_var_name)

// Note: ActionID here is index within `Action` variant, not the `EmptyAction` variant.
const map<ActionID, string> ShortcutForId = {
    {id<Undo>, "cmd+z"},
    {id<Redo>, "shift+cmd+z"},
    {id<OpenEmptyProject>, "cmd+n"},
    {id<ShowOpenProjectDialog>, "cmd+o"},
    {id<OpenDefaultProject>, "shift+cmd+o"},
    {id<SaveCurrentProject>, "cmd+s"},
    {id<ShowSaveProjectDialog>, "shift+cmd+s"},
};

constexpr ActionID GetId(const Action &action) { return action.index(); }
constexpr ActionID GetId(const StateAction &action) { return action.index(); }

string GetName(const ProjectAction &action);
string GetName(const StateAction &action);
string GetShortcut(const EmptyAction &);
string GetMenuLabel(const EmptyAction &);
Gesture MergeGesture(const Gesture &);
} // namespace action
