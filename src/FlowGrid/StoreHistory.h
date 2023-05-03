#pragma once

#include "Actions.h"
#include "Store.h"

enum Direction {
    Forward,
    Reverse
};

using namespace action;

struct StoreHistory {
    struct ReferenceRecord {
        const TimePoint Committed;
        const Store &Store; // Reference to the store as it was at `Committed` time
        const Gesture &Gesture; // Reference to the compressed gesture (list of `ActionMoment`s) that caused the store change
    };

    struct Plottable {
        vector<const char *> Labels;
        vector<ImU64> Values;
    };

    StoreHistory(const Store &);
    ~StoreHistory() = default;

    void Reset(const Store &);

    void UpdateGesturePaths(const Gesture &, const Patch &);
    Plottable StatePathUpdateFrequencyPlottable() const;
    std::optional<TimePoint> LatestUpdateTime(const StatePath &path) const;

    void FinalizeGesture();
    void SetIndex(Count);
    void Add(TimePoint, const Store &, const Gesture &);

    Count Size() const;
    bool Empty() const;
    bool CanUndo() const;
    bool CanRedo() const;

    const Store &CurrentStore() const;
    Patch CreatePatch(Count index) const; // Create a patch between the store at `index` and the store at `index - 1`.
    ReferenceRecord RecordAt(Count index) const;
    Gestures Gestures() const;
    TimePoint GestureStartTime() const;
    float GestureTimeRemainingSec(float gesture_duration_sec) const;

    Count Index{0};
    Gesture ActiveGesture{}; // uncompressed, uncommitted
    vector<StatePath> LatestUpdatedPaths{};
    std::unordered_map<StatePath, vector<TimePoint>, StatePathHash> CommittedUpdateTimesForPath{};

private:
    std::unordered_map<StatePath, vector<TimePoint>, StatePathHash> GestureUpdateTimesForPath{};
};

extern StoreHistory History; // One store checkpoint for every gesture.
