#pragma once

#include "Actions.h"
#include "Config.h"
#include "Store.h"

#include "immer/map.hpp"
#include "nlohmann/json_fwd.hpp"

//-----------------------------------------------------------------------------
// [SECTION] History
//-----------------------------------------------------------------------------

enum Direction {
    Forward,
    Reverse
};

using namespace action;

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
    std::unordered_map<StatePath, vector<TimePoint>, StatePathHash> CommittedUpdateTimesForPath{};

private:
    std::unordered_map<StatePath, vector<TimePoint>, StatePathHash> GestureUpdateTimesForPath{};
};

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

struct Context {
    static bool IsUserProjectPath(const fs::path &);

    nlohmann::json GetProjectJson(ProjectFormat format = StateFormat);
    void SaveEmptyProject();
    void OpenProject(const fs::path &);
    bool SaveProject(const fs::path &);
    void SaveCurrentProject();

    void RunQueuedActions(bool force_finalize_gesture = false);
    bool ActionAllowed(ID) const;
    bool ActionAllowed(const Action &) const;
    bool ActionAllowed(const EmptyAction &) const;

    void Clear();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    Patch SetStore(const Store &);

public:
    bool ProjectHasChanges{false};

private:
    void ApplyAction(const ProjectAction &);

    void SetCurrentProjectPath(const fs::path &);

    std::optional<fs::path> CurrentProjectPath;
};

extern Context c; // Context global, initialized in `main.cpp`.
extern StoreHistory History; // One store checkpoint for every gesture.
