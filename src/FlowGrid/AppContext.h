#pragma once

#include "App.h"

#include <map>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <set>

//-----------------------------------------------------------------------------
// [SECTION] Configuration constants
//-----------------------------------------------------------------------------

inline static const unordered_map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
inline static const auto ProjectFormatForExtension = ExtensionForProjectFormat | transform([](const auto &p) { return pair(p.second, p.first); }) | to<std::map>();
inline static const auto AllProjectExtensions = views::keys(ProjectFormatForExtension) | to<std::set>;
inline static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
inline static const string PreferencesFileExtension = ".flp";
inline static const string FaustDspFileExtension = ".dsp";

inline static const fs::path InternalPath = ".flowgrid";
inline static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
inline static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(ActionFormat));
inline static const fs::path PreferencesPath = InternalPath / ("Preferences" + PreferencesFileExtension);

//-----------------------------------------------------------------------------
// [SECTION] History
//-----------------------------------------------------------------------------

enum Direction {
    Forward,
    Reverse
};

struct StoreHistory {
    struct Record {
        const TimePoint Committed;
        const Store Store; // The store as it was at `Committed` time
        const Gesture Gesture; // Compressed gesture (list of `ActionMoment`s) that caused the store change
    };
    struct Plottable {
        vector<const char *> Labels;
        vector<ImU64> Values;
    };

    StoreHistory(const Store &store) : Records{{Clock::now(), store, {}}} {}

    void UpdateGesturePaths(const Gesture &, const Patch &);
    Plottable StatePathUpdateFrequencyPlottable() const;
    std::optional<TimePoint> LatestUpdateTime(const StatePath &path) const;

    void FinalizeGesture();
    void SetIndex(Count);

    Count Size() const;
    bool Empty() const;
    bool CanUndo() const;
    bool CanRedo() const;

    Gestures Gestures() const;
    TimePoint GestureStartTime() const;
    float GestureTimeRemainingSec() const;

    Count Index{0};
    vector<Record> Records;
    Gesture ActiveGesture{}; // uncompressed, uncommitted
    vector<StatePath> LatestUpdatedPaths{};
    unordered_map<StatePath, vector<TimePoint>, StatePathHash> CommittedUpdateTimesForPath{};

private:
    unordered_map<StatePath, vector<TimePoint>, StatePathHash> GestureUpdateTimesForPath{};
};

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

struct Context {
    Context();
    ~Context();

    static bool IsUserProjectPath(const fs::path &);
    json GetProjectJson(ProjectFormat format = StateFormat);
    void SaveEmptyProject();
    void OpenProject(const fs::path &);
    bool SaveProject(const fs::path &);
    void SaveCurrentProject();

    void RunQueuedActions(bool force_finalize_gesture = false);
    bool ActionAllowed(ID) const;
    bool ActionAllowed(const Action &) const;
    bool ActionAllowed(const EmptyAction &) const;

    bool ClearPreferences();
    void Clear();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    Patch SetStore(const Store &);

    TransientStore InitStore{}; // Used in `StateMember` constructors to initialize the store.

private:
    const State ApplicationState{};
    Store ApplicationStore{InitStore.persistent()}; // Create the local canonical store, initially containing the full application state constructed by `State`.

public:
    const State &s = ApplicationState;
    const Store &AppStore = ApplicationStore;

    Preferences Preferences;
    StoreHistory History{AppStore}; // One store checkpoint for every gesture.
    bool ProjectHasChanges{false};

private:
    void ApplyAction(const ProjectAction &);

    void SetCurrentProjectPath(const fs::path &);
    bool WritePreferences() const;

    std::optional<fs::path> CurrentProjectPath;
};

//-----------------------------------------------------------------------------
// [SECTION] Globals
//-----------------------------------------------------------------------------

/**
Declare read-only accessors for:
 - The global state instance `state` (and its shorthand, `s`)
 - The global context instance `context` (and its shorthand, `c`)

The state & context instances are initialized and instantiated in `main.cpp`.

`s` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`s` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see [Application Architecture](https://github.com/khiner/flowgrid#application-architecture) for more details.)

Usage example:

```cpp
// Get the canonical application audio state:
const Audio &audio = s.Audio;

// Get the currently active gesture (collection of actions) from the global application context:
 const Gesture &ActiveGesture = c.ActiveGesture;
```
*/

extern const State &s;
extern Context c;
extern const Store &AppStore; // Read-only global for full, read-only canonical application state.
