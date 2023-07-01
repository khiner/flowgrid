#include "StoreHistory.h"

#include <range/v3/range/conversion.hpp>

#include "Store.h"
#include "StoreImpl.h"

using std::string;

struct Record {
    const Store Store;
    const Gesture Gesture;
};

static std::vector<Record> Records;

StoreHistory::StoreHistory() {
    Records.clear();
    Records.emplace_back(store::Get(), Gesture{{}, Clock::now()});
}
StoreHistory::~StoreHistory() {
    // Not clearing records here because the destructor for the old singleton instance is called after the new instance is constructed.
    // This is fine, since `Records` should have the same lifetime as the application.
}

void StoreHistory::Add(const Store &store, const Gesture &gesture) {
    const auto patch = store::CreatePatch(CurrentStore(), store);
    if (patch.Empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.emplace_back(store, gesture);
    Index = Size() - 1;
    for (const auto &path : patch.GetPaths()) CommitTimesForPath[path].emplace_back(gesture.CommitTime);
}

void StoreHistory::AddTransientGesture(const Gesture &gesture) {
    Add(store::GetPersistent(), gesture);
}

Count StoreHistory::Size() const { return Records.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

const Store &StoreHistory::CurrentStore() const { return Records[Index].Store; }
Patch StoreHistory::CreatePatch(Count index) const { return store::CreatePatch(Records[index - 1].Store, Records[index].Store); }

StoreHistory::ReferenceRecord StoreHistory::RecordAt(Count index) const {
    const auto &[store, gesture] = Records[index];
    return {store, gesture};
}

StoreHistory::IndexedGestures StoreHistory::GetIndexedGestures() const {
    // All recorded gestures except the first, since the first record only holds the initial store with no gestures.
    Gestures gestures = Records | std::views::drop(1) | std::views::transform([](const auto &record) { return record.Gesture; }) | ranges::to<std::vector>;
    return {std::move(gestures), Index};
}

void StoreHistory::CommitGesture(Gesture &&gesture) {
    Add(store::Get(), std::move(gesture));
}

void StoreHistory::SetIndex(Count new_index) {
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    const Count old_index = Index;
    Index = new_index;

    // TODO
    // Turn the `CommitTimesForPath` map into an `immer/map`, and keep a separate vector (soon, tree) of them, in parallel to `Records`.
    //     using TimesForPath = immer::map<StorePath, std::vector<TimePoint>, PathHash>
    //     struct MetricsRecord{ TimesForPath CommitTimesForPath; };
    //     using MetricsRecords = vector<MetricsRecord>;
    const auto direction = new_index > old_index ? Forward : Reverse;
    auto i = int(old_index);
    while (i != int(new_index)) {
        const int history_index = direction == Reverse ? --i : i++;
        const Count record_index = history_index == -1 ? Index : history_index;
        const auto &segment_patch = CreatePatch(record_index + 1);
        const auto &gesture_commit_time = Records[record_index + 1].Gesture.CommitTime;
        for (const auto &path : segment_patch.GetPaths()) {
            if (direction == Forward) {
                CommitTimesForPath[path].emplace_back(gesture_commit_time);
            } else if (CommitTimesForPath.contains(path)) {
                auto &update_times = CommitTimesForPath.at(path);
                update_times.pop_back();
                if (update_times.empty()) CommitTimesForPath.erase(path);
            }
        }
    }
}

StoreHistory History{}; // Global.
