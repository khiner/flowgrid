#include "StoreHistory.h"

#include <ranges>

#include "immer/map_transient.hpp"
#include "immer/vector.hpp"
#include "immer/vector_transient.hpp"

#include "Core/Component.h"
#include "Store.h"

using std::ranges::to, std::views::drop, std::views::transform;

struct StoreHistory::Metrics {
    immer::map<ID, immer::vector<TimePoint>> CommitTimesById;

    void AddPatch(const Patch &patch, const TimePoint &commit_time) {
        for (ID id : patch.GetIds()) {
            auto commit_times = CommitTimesById.count(id) ? CommitTimesById.at(id).push_back(commit_time) : immer::vector<TimePoint>{commit_time};
            CommitTimesById = CommitTimesById.set(id, std::move(commit_times));
        }
    }
};

struct Record {
    Store Store;
    Gesture Gesture;
    StoreHistory::Metrics Metrics;
};

struct StoreHistory::Records {
    Records(const ::Store &initial_store) : Value{{initial_store, Gesture{{}, Clock::now()}, StoreHistory::Metrics{{}}}} {}

    std::vector<Record> Value;
};

StoreHistory::StoreHistory(const ::Store &store)
    : Store(store), _Records(std::make_unique<Records>(Store)), _Metrics(std::make_unique<Metrics>()) {}

StoreHistory::~StoreHistory() = default;

void StoreHistory::Clear() {
    Index = 0;
    _Records = std::make_unique<Records>(Store);
    _Metrics = std::make_unique<Metrics>();
}

void StoreHistory::AddGesture(Gesture &&gesture, ID component_id) {
    const auto store_impl = Store;
    const auto patch = Store.CreatePatch(CurrentStore(), store_impl, component_id);
    if (patch.Empty()) return;

    _Metrics->AddPatch(patch, gesture.CommitTime);

    while (Size() > Index + 1) _Records->Value.pop_back(); // TODO use an undo _tree_ and keep this history
    _Records->Value.emplace_back(std::move(store_impl), std::move(gesture), *_Metrics);
    Index = Size() - 1;
}

u32 StoreHistory::Size() const { return _Records->Value.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

const Store &StoreHistory::CurrentStore() const { return _Records->Value[Index].Store; }

std::map<ID, u32> StoreHistory::GetChangeCountById() const {
    return _Records->Value[Index].Metrics.CommitTimesById |
        transform([](const auto &entry) { return std::pair(entry.first, entry.second.size()); }) |
        to<std::map<ID, u32>>();
}

u32 StoreHistory::GetChangedPathsCount() const { return _Records->Value[Index].Metrics.CommitTimesById.size(); }

Patch StoreHistory::CreatePatch(u32 index, ID component_id) const {
    return Store.CreatePatch(_Records->Value[index - 1].Store, _Records->Value[index].Store, component_id);
}

StoreHistory::ReferenceRecord StoreHistory::RecordAt(u32 index) const {
    const auto &record = _Records->Value[index];
    return {record.Store, record.Gesture};
}

StoreHistory::IndexedGestures StoreHistory::GetIndexedGestures() const {
    // All recorded gestures except the first, since the first record only holds the initial store with no gestures.
    Gestures gestures = _Records->Value | drop(1) | transform([](const auto &record) { return record.Gesture; }) | to<std::vector>();
    return {std::move(gestures), Index};
}

void StoreHistory::SetIndex(u32 new_index) {
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    Index = new_index;
    _Metrics = std::make_unique<Metrics>(_Records->Value[Index].Metrics);
}

extern StoreHistory &History; // Global.
