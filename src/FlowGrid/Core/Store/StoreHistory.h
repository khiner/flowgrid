#pragma once

#include "Core/Action/Actions.h"

struct Store;

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
        const TimePoint GestureCommitTime; // The time at which the store was committed.
        const Store &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the compressed gesture (list of `ActionMoment`s) that caused the store change
    };

    struct Plottable {
        std::vector<const char *> Labels;
        std::vector<ImU64> Values;
    };

    StoreHistory();
    ~StoreHistory();

    void SetIndex(Count);

    void AddTransient(const Gesture &); // Only used during action-formmated project loading.
    void CommitGesture();
    void OnStoreCommit(const TimePoint &commit_time, const std::vector<Action::SavableActionMoment> &, const Patch &);

    Count Size() const;
    bool Empty() const;

    bool CanUndo() const;
    bool CanRedo() const;

    const Store &CurrentStore() const;
    Patch CreatePatch(Count index) const; // Create a patch between the store at `index` and the store at `index - 1`.
    ReferenceRecord RecordAt(Count index) const;
    IndexedGestures GetIndexedGestures() const; // An action-formmatted project is the result of this method converted directly to JSON.

    TimePoint GestureStartTime() const;
    float GestureTimeRemainingSec(float gesture_duration_sec) const;
    Plottable StorePathChangeFrequencyPlottable() const;
    std::optional<TimePoint> LatestUpdateTime(const StorePath &) const;

    Count Index{0};
    Gesture ActiveGesture{}; // uncompressed, uncommitted
    std::vector<StorePath> LatestUpdatedPaths{};

private:
    using TimesForPath = std::unordered_map<StorePath, std::vector<TimePoint>, PathHash>;
    TimesForPath CommitTimesForPath{};
    TimesForPath GestureUpdateTimesForPath{};

    void Add(TimePoint, const Store &, const Gesture &);
};

Json(StoreHistory::IndexedGestures, Gestures, Index);

extern StoreHistory History; // One store checkpoint for every gesture.
