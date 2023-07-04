#pragma once

#include "Core/Action/Actions.h"
#include "Helper/Paths.h"

struct StoreImpl;

enum Direction {
    Forward,
    Reverse
};

struct StoreHistoryMetrics;

struct StoreHistory {
    struct Metrics;

    // Used for saving/loading the history.
    // This is all the information needed to reconstruct a project.
    struct IndexedGestures {
        Gestures Gestures;
        Count Index;
    };

    struct ReferenceRecord {
        const StoreImpl &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the (compressed) gesture that caused the store change.
    };

    StoreHistory();
    ~StoreHistory();

    void AddGesture(Gesture &&); // Add a gesture to the history.
    void SetIndex(Count);

    Count Size() const;
    bool Empty() const;
    bool CanUndo() const;
    bool CanRedo() const;

    const StoreImpl &CurrentStore() const;
    Patch CreatePatch(Count index) const; // Create a patch between the store at `index` and the store at `index - 1`.
    ReferenceRecord RecordAt(Count index) const;
    IndexedGestures GetIndexedGestures() const; // An action-formmatted project is the result of this method converted directly to JSON.
    std::map<StorePath, Count> GetChangeCountByPath() const; // Ordered by path.
    Count GetChangedPathsCount() const;

    Count Index{0};
};

Json(StoreHistory::IndexedGestures, Gestures, Index);

extern StoreHistory History; // One store checkpoint for every gesture.
