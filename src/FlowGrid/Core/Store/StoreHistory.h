#pragma once

#include "Core/Action/Action.h"
#include "StoreFwd.h"

enum Direction {
    Forward,
    Reverse
};

using Action::Gesture, Action::Gestures;

struct StoreHistory {
    // Used for saving/loading the history.
    // This is all the information needed to reconstruct a project.
    struct IndexedGestures {
        Gestures Gestures;
        Count Index;
    };

    struct ReferenceRecord {
        const TimePoint Committed; // The time at which the store was committed.
        const Store &Store; // Reference to the store as it was at `Committed` time
        const Gesture &Gesture; // Reference to the compressed gesture (list of `ActionMoment`s) that caused the store change
    };

    struct Plottable {
        std::vector<const char *> Labels;
        std::vector<ImU64> Values;
    };

    StoreHistory();
    ~StoreHistory();

    void UpdateGesturePaths(const Gesture &, const Patch &);
    Plottable StorePathUpdateFrequencyPlottable() const;
    std::optional<TimePoint> LatestUpdateTime(const StorePath &path) const;

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
    IndexedGestures GetIndexedGestures() const;
    TimePoint GestureStartTime() const;
    float GestureTimeRemainingSec(float gesture_duration_sec) const;

    Count Index{0};
    Gesture ActiveGesture{}; // uncompressed, uncommitted
    std::vector<StorePath> LatestUpdatedPaths{};

private:
    using TimesForPath = std::unordered_map<StorePath, std::vector<TimePoint>, StorePathHash>;
    TimesForPath CommittedUpdateTimesForPath{};
    TimesForPath GestureUpdateTimesForPath{};
};

Json(StoreHistory::IndexedGestures, Gestures, Index);

extern StoreHistory History; // One store checkpoint for every gesture.
