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
    Gestures GetGestures() const;

    std::map<ID, u32> GetChangeCountById() const; // Ordered by path.
    u32 GetChangedPathsCount() const;

    u32 Index{0};

private:
    std::unique_ptr<Records> _Records;
    std::unique_ptr<Metrics> _Metrics;
};
