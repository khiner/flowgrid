#pragma once

#include <map>

#include "nlohmann/json_fwd.hpp"

#include "Core/Primitive/Scalar.h"

using json = nlohmann::json;

struct Store;
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
        const Store &Store; // Reference to the store as it was at `GestureCommitTime`.
        const Gesture &Gesture; // Reference to the (compressed) gesture that caused the store change.
    };

    StoreHistory(const Store &);
    ~StoreHistory();

    u32 Size() const;
    bool Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
    bool CanUndo() const { return Index > 0; }
    bool CanRedo() const { return Index < Size() - 1; }

    void AddGesture(Store, Gesture &&, ID component_id);
    void Clear(const Store &);
    void SetIndex(u32);

    const Store &CurrentStore() const;
    const Store &PrevStore() const;

    ReferenceRecord At(u32 index) const;
    Gestures GetGestures() const;

    std::map<ID, u32> GetChangeCountById() const; // Ordered by path.
    u32 GetChangedPathsCount() const;

    u32 Index{0};

private:
    std::unique_ptr<Records> _Records;
    std::unique_ptr<Metrics> _Metrics;
};
