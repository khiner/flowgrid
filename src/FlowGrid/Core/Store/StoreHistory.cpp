#include "StoreHistory.h"

#include <range/v3/range/conversion.hpp>

#include "immer/map_transient.hpp"
#include "immer/vector.hpp"
#include "immer/vector_transient.hpp"

#include "Store.h"
#include "StoreImpl.h"

using std::string;

struct StoreHistory::Metrics {
    immer::map<StorePath, immer::vector<TimePoint>, PathHash> CommitTimesByPath;

    void AddPatch(const Patch &patch, const TimePoint &commit_time) {
        for (const auto &path : patch.GetPaths()) {
            immer::vector<TimePoint> commit_times = CommitTimesByPath.count(path) == 0 ? immer::vector<TimePoint>{} : CommitTimesByPath.at(path);
            commit_times = commit_times.push_back(commit_time);
            CommitTimesByPath = CommitTimesByPath.set(path, commit_times);
        }
    }
};

struct Record {
    StoreImpl Store;
    Gesture Gesture;
    StoreHistory::Metrics Metrics;
};

static std::vector<Record> Records;

StoreHistory::StoreHistory(const ::Store &store)
    : Store(store), HistoryMetrics(std::make_unique<Metrics>()) {
    Records.clear();
    Records.emplace_back(Store.Get(), Gesture{{}, Clock::now()}, Metrics{{}});
}

StoreHistory::~StoreHistory() {
    // Not clearing here because the destructor for the old singleton instance is called after the new instance is constructed.
    // This is fine, since records should have the same lifetime as the application.
}

void StoreHistory::Clear() {
    Index = 0;
    HistoryMetrics = std::make_unique<Metrics>();
    Records.clear();
    Records.emplace_back(Store.Get(), Gesture{{}, Clock::now()}, Metrics{{}});
}

void StoreHistory::AddGesture(Gesture &&gesture) {
    const auto store_impl = Store.Get();
    const auto patch = Store.CreatePatch(this->CurrentStore(), store_impl);
    if (patch.Empty()) return;

    HistoryMetrics->AddPatch(patch, gesture.CommitTime);

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.emplace_back(store_impl, std::move(gesture), *HistoryMetrics);
    Index = Size() - 1;
}

Count StoreHistory::Size() const { return Records.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

const StoreImpl &StoreHistory::CurrentStore() const { return Records[Index].Store; }

std::map<StorePath, Count> StoreHistory::GetChangeCountByPath() const {
    return Records[Index].Metrics.CommitTimesByPath |
        std::views::transform([](const auto &entry) { return std::pair(entry.first, entry.second.size()); }) |
        ranges::to<std::map<StorePath, Count>>;
}

Count StoreHistory::GetChangedPathsCount() const { return Records[Index].Metrics.CommitTimesByPath.size(); }

Patch StoreHistory::CreatePatch(Count index) const {
    return Store.CreatePatch(Records[index - 1].Store, Records[index].Store);
}

StoreHistory::ReferenceRecord StoreHistory::RecordAt(Count index) const {
    const auto &record = Records[index];
    return {record.Store, record.Gesture};
}

StoreHistory::IndexedGestures StoreHistory::GetIndexedGestures() const {
    // All recorded gestures except the first, since the first record only holds the initial store with no gestures.
    Gestures gestures = Records | std::views::drop(1) | std::views::transform([](const auto &record) { return record.Gesture; }) | ranges::to<std::vector>;
    return {std::move(gestures), Index};
}

void StoreHistory::SetIndex(Count new_index) {
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    Index = new_index;
    HistoryMetrics = std::make_unique<Metrics>(Records[Index].Metrics);
}

extern StoreHistory &History; // Global.
