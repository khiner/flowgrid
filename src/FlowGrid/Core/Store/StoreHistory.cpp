#include "StoreHistory.h"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>
#include <set>

#include "Store.h"
#include "StoreImpl.h"

using std::string, std::vector;

struct Record {
    const TimePoint GestureCommitTime;
    const Store Store;
    const Gesture Gesture;
};

static vector<Record> Records;

StoreHistory::StoreHistory() {
    Records.clear();
    Records.emplace_back(Clock::now(), store::Get(), Gesture{});
}
StoreHistory::~StoreHistory() {
    // Not clearing records here because the destructor for the old singleton instance is called after the new instance is constructed.
    // This is fine, since `Records` should have the same lifetime as the application.
}

void StoreHistory::Add(TimePoint gesture_commit_time, const Store &store, const Gesture &gesture) {
    const auto patch = store::CreatePatch(CurrentStore(), store);
    if (patch.Empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.emplace_back(gesture_commit_time, store, gesture);
    Index = Size() - 1;
    for (const auto &[partial_path, op] : patch.Ops) CommitTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_commit_time);
}

void StoreHistory::AddTransient(const Gesture &gesture) {
    Add(gesture.back().second, store::GetPersistent(), gesture); // todo save/load gesture commit times
}

void StoreHistory::OnStoreCommit(const TimePoint &commit_time, const std::vector<Action::SavableActionMoment> &actions, const Patch &patch) {
    ActiveGesture.insert(ActiveGesture.end(), actions.begin(), actions.end());
    for (const auto &[partial_path, op] : patch.Ops) GestureUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(commit_time);
}

Count StoreHistory::Size() const { return Records.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return !ActiveGesture.empty() || Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

const Store &StoreHistory::CurrentStore() const { return Records[Index].Store; }
Patch StoreHistory::CreatePatch(Count index) const { return store::CreatePatch(Records[index - 1].Store, Records[index].Store); }

StoreHistory::ReferenceRecord StoreHistory::RecordAt(Count index) const {
    const auto &[time, store, gesture] = Records[index];
    return {time, store, gesture};
}

StoreHistory::IndexedGestures StoreHistory::GetIndexedGestures() const {
    const Gestures gestures = Records | std::views::transform([](const auto &record) { return record.Gesture; }) |
        std::views::filter([](const auto &gesture) { return !gesture.empty(); }) | ranges::to<vector>; // First gesture is expected to be empty.
    return {gestures, Index};
}

TimePoint StoreHistory::GestureStartTime() const {
    if (ActiveGesture.empty()) return {};
    return ActiveGesture.back().second;
}

float StoreHistory::GestureTimeRemainingSec(float gesture_duration_sec) const {
    if (ActiveGesture.empty()) return 0;
    return std::max(0.f, gesture_duration_sec - fsec(Clock::now() - GestureStartTime()).count());
}

static Gesture MergeGesture(const Gesture &gesture) {
    Gesture merged_gesture; // Mutable return value.

    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const Action::SavableActionMoment> active;
    for (Count i = 0; i < gesture.size(); i++) {
        if (!active) active.emplace(gesture[i]);
        const auto &a = *active;
        const auto &b = gesture[i + 1];
        const auto merge_result = a.first.Merge(b.first);
        Visit(
            merge_result,
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_gesture.emplace_back(a); //
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const Action::Savable &merged_action) {
                // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
                active.emplace(merged_action, b.second);
            },
        );
    }
    if (active) merged_gesture.emplace_back(*active);

    return merged_gesture;
}

void StoreHistory::CommitGesture() {
    GestureUpdateTimesForPath.clear();
    if (ActiveGesture.empty()) return;

    const auto gesture = MergeGesture(ActiveGesture);
    ActiveGesture.clear();
    if (gesture.empty()) return;

    Add(Clock::now(), store::Get(), gesture);
}

std::optional<TimePoint> StoreHistory::LatestUpdateTime(const StorePath &path) const {
    if (GestureUpdateTimesForPath.contains(path)) return GestureUpdateTimesForPath.at(path).back();
    if (CommitTimesForPath.contains(path)) return CommitTimesForPath.at(path).back();
    return {};
}

StoreHistory::Plottable StoreHistory::StorePathChangeFrequencyPlottable() const {
    if (CommitTimesForPath.empty() && GestureUpdateTimesForPath.empty()) return {};

    const std::set<StorePath> paths =
        ranges::views::concat(
            ranges::views::keys(CommitTimesForPath),
            ranges::views::keys(GestureUpdateTimesForPath)
        ) |
        ranges::to<std::set>;

    const bool has_gesture = !GestureUpdateTimesForPath.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    Count i = 0;
    for (const auto &path : paths) values[i++] = CommitTimesForPath.contains(path) ? CommitTimesForPath.at(path).size() : 0;
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture) {
        for (const auto &path : paths) {
            values[i++] = GestureUpdateTimesForPath.contains(path) ? GestureUpdateTimesForPath.at(path).size() : 0;
        }
    }

    const auto labels = paths | std::views::transform([](const string &path) {
                            // Convert `string` to char array, removing first character of the path, which is a '/'.
                            char *label = new char[path.size()];
                            std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
                            return label;
                        }) |
        ranges::to<vector<const char *>>;

    return {labels, values};
}

void StoreHistory::SetIndex(Count new_index) {
    // If we're mid-gesture, revert the current gesture before navigating to the new index.
    ActiveGesture.clear();
    GestureUpdateTimesForPath.clear();
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
        const auto &gesture_time = Records[record_index + 1].Gesture.back().second;
        for (const auto &[partial_path, _] : segment_patch.Ops) {
            const auto &path = segment_patch.BasePath / partial_path;
            if (direction == Forward) {
                CommitTimesForPath[path].emplace_back(gesture_time);
            } else if (CommitTimesForPath.contains(path)) {
                auto &update_times = CommitTimesForPath.at(path);
                update_times.pop_back();
                if (update_times.empty()) CommitTimesForPath.erase(path);
            }
        }
    }
}

StoreHistory History{}; // Global.
