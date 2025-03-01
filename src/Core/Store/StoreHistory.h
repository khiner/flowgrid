#pragma once

#include <map>
#include <memory>

#include "nlohmann/json_fwd.hpp"

#include "Core/Scalar.h"

using json = nlohmann::json;

struct PersistentStore;
struct Gesture;
using Gestures = std::vector<Gesture>;

enum Direction {
    Forward,
    Reverse
};

struct StoreHistory {
    struct Records;
    struct Metrics;

    struct ReferenceRecord {
        const PersistentStore &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the (compressed) gesture that caused the store change.
    };

    StoreHistory(const PersistentStore &);
    ~StoreHistory();

    u32 Size() const;
    bool Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
    bool CanUndo() const { return Index > 0; }
    bool CanRedo() const { return Index < Size() - 1; }

    void AddGesture(PersistentStore, Gesture &&, ID component_id);
    void Clear(const PersistentStore &);
    void SetIndex(u32);

    const PersistentStore &CurrentStore() const;
    const PersistentStore &PrevStore() const;

    ReferenceRecord At(u32 index) const;
    Gestures GetGestures() const;

    std::map<ID, u32> GetChangeCountById() const; // Ordered by path.
    u32 GetChangedPathsCount() const;

    u32 Index{0};

private:
    std::unique_ptr<Records> _Records;
    std::unique_ptr<Metrics> _Metrics;
};
