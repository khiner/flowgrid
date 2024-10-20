#include "StoreHistory.h"

#include <ranges>

#include "Core/Action/Actions.h"
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
    : _Records(std::make_unique<Records>(store)), _Metrics(std::make_unique<Metrics>()) {}

StoreHistory::~StoreHistory() = default;

void StoreHistory::Clear(const Store &store) {
    Index = 0;
    _Records = std::make_unique<Records>(store);
    _Metrics = std::make_unique<Metrics>();
}

void StoreHistory::AddGesture(Store store, Gesture &&gesture, ID component_id) {
    const auto patch = store.CreatePatch(CurrentStore(), component_id);
    if (patch.Empty()) return;

    _Metrics->AddPatch(patch, gesture.CommitTime);

    while (Size() > Index + 1) _Records->Value.pop_back(); // todo use an undo _tree_ and keep this history
    _Records->Value.emplace_back(std::move(store), std::move(gesture), *_Metrics);
    Index = Size() - 1;
}

u32 StoreHistory::Size() const { return _Records->Value.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

const Store &StoreHistory::CurrentStore() const { return _Records->Value[Index].Store; }
const Store &StoreHistory::PrevStore() const { return _Records->Value[Index - 1].Store; }

std::map<ID, u32> StoreHistory::GetChangeCountById() const {
    return _Records->Value[Index].Metrics.CommitTimesById |
        transform([](const auto &entry) { return std::pair(entry.first, entry.second.size()); }) |
        to<std::map<ID, u32>>();
}

u32 StoreHistory::GetChangedPathsCount() const { return _Records->Value[Index].Metrics.CommitTimesById.size(); }

StoreHistory::ReferenceRecord StoreHistory::At(u32 index) const {
    const auto &record = _Records->Value[index];
    return {record.Store, record.Gesture};
}

Gestures StoreHistory::GetGestures() const {
    // The first record only holds the initial store with no gestures.
    return _Records->Value | drop(1) | transform([](const auto &record) { return record.Gesture; }) | to<std::vector>();
}

void StoreHistory::SetIndex(u32 new_index) {
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    Index = new_index;
    _Metrics = std::make_unique<Metrics>(_Records->Value[Index].Metrics);
}

extern StoreHistory &History; // Global.
