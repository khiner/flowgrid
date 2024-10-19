#pragma once

#include "Core/Action/Actions.h"
#include "Core/Helper/Path.h"

struct Store;

enum Direction {
    Forward,
    Reverse
};

struct StoreHistory {
    struct Records;
    struct Metrics;

    // Used for saving/loading the history.
    // This is all the information needed to reconstruct a project.
    struct IndexedGestures {
        Gestures Gestures;
        u32 Index;
    };

    struct ReferenceRecord {
        const Store &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the (compressed) gesture that caused the store change.
    };

    StoreHistory(const Store &);
    ~StoreHistory();

    void Clear(const Store &);
    void AddGesture(Store, Gesture &&, ID component_id);
    void SetIndex(u32);

    u32 Size() const;
    bool Empty() const;
    bool CanUndo() const;
    bool CanRedo() const;

    const Store &CurrentStore() const;
    const Store &PrevStore() const;

    ReferenceRecord At(u32 index) const;
    IndexedGestures GetIndexedGestures() const; // An action-formmatted project is the result of this method converted directly to JSON.
    std::map<ID, u32> GetChangeCountById() const; // Ordered by path.
    u32 GetChangedPathsCount() const;

    u32 Index{0};

private:
    std::unique_ptr<Records> _Records;
    std::unique_ptr<Metrics> _Metrics;
};

Json(StoreHistory::IndexedGestures, Gestures, Index);
