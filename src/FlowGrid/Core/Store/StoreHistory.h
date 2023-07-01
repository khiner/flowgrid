#pragma once

#include "Core/Action/Actions.h"
#include "Helper/Time.h"

struct Store;

enum Direction {
    Forward,
    Reverse
};

struct StoreHistory {
    // Used for saving/loading the history.
    // This is all the information needed to reconstruct a project.
    struct IndexedGestures {
        Gestures Gestures;
        Count Index;
    };

    struct ReferenceRecord {
        const Store &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the (compressed) gesture that caused the store change.
    };

    StoreHistory();
    ~StoreHistory();

    void SetIndex(Count);

    void AddTransientGesture(const Gesture &); // Only used during action-formmated project loading.
    void CommitGesture(Gesture &&); // Add a gesture to the history.

    Count Size() const;
    bool Empty() const;

    bool CanUndo() const;
    bool CanRedo() const;

    const Store &CurrentStore() const;
    Patch CreatePatch(Count index) const; // Create a patch between the store at `index` and the store at `index - 1`.
    ReferenceRecord RecordAt(Count index) const;
    IndexedGestures GetIndexedGestures() const; // An action-formmatted project is the result of this method converted directly to JSON.

    Count Index{0};
    Patch LatestPatch;

    std::unordered_map<StorePath, std::vector<TimePoint>, PathHash> CommitTimesForPath{};

private:
    void Add(const Store &, const Gesture &);
};

Json(StoreHistory::IndexedGestures, Gestures, Index);

extern StoreHistory History; // One store checkpoint for every gesture.
