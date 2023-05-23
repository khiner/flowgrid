#include "StoreHistory.h"

#include "immer/map.hpp"
#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <set>

#include "Store.h"

namespace views = ranges::views;
using ranges::to, views::transform;

struct Record {
    const TimePoint Committed;
    const Store Store; // The store as it was at `Committed` time
    const Gesture Gesture; // Compressed gesture (list of action/timestamp pairs) that caused the store change
};

static vector<Record> Records;

StoreHistory::StoreHistory(const Store &store) {
    Reset(store);
}

void StoreHistory::Reset(const Store &store) {
    Records.clear();
    Records.push_back({Clock::now(), store, {}});
}

void StoreHistory::Add(TimePoint time, const Store &store, const Gesture &gesture) {
    Records.push_back({time, store, gesture});
    Index = Size() - 1;
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

Gestures StoreHistory::Gestures() const {
    return Records | transform([](const auto &record) { return record.Gesture; }) |
        views::filter([](const auto &gesture) { return !gesture.empty(); }) | to<vector>; // First gesture is expected to be empty.
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
    Gesture merged_gesture; // Mutable return value

    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const Action::StatefulActionMoment> active;
    for (Count i = 0; i < gesture.size(); i++) {
        if (!active) active.emplace(gesture[i]);
        const auto &a = *active;
        const auto &b = gesture[i + 1];
        const auto merge_result = a.first.Merge(b.first);
        Match(
            merge_result,
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_gesture.emplace_back(a); //
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const Action::StatefulAction &merged_action) {
                active.emplace(merged_action, b.second); // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
            },
        );
    }
    if (active) merged_gesture.emplace_back(*active);

    return merged_gesture;
}

void StoreHistory::FinalizeGesture() {
    if (ActiveGesture.empty()) return;

    const auto merged_gesture = MergeGesture(ActiveGesture);
    ActiveGesture.clear();
    GestureUpdateTimesForPath.clear();
    if (merged_gesture.empty()) return;

    const auto &patch = store::CreatePatch(AppStore, Records[Index].Store);
    if (patch.Empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Add(Clock::now(), AppStore, merged_gesture);
    const auto &gesture_time = merged_gesture.back().second;
    for (const auto &[partial_path, op] : patch.Ops) CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
}

void StoreHistory::UpdateGesturePaths(const Gesture &gesture, const Patch &patch) {
    const auto &gesture_time = gesture.back().second;
    for (const auto &[partial_path, op] : patch.Ops) GestureUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
}

std::optional<TimePoint> StoreHistory::LatestUpdateTime(const StorePath &path) const {
    if (GestureUpdateTimesForPath.contains(path)) return GestureUpdateTimesForPath.at(path).back();
    if (CommittedUpdateTimesForPath.contains(path)) return CommittedUpdateTimesForPath.at(path).back();
    return {};
}

StoreHistory::Plottable StoreHistory::StorePathUpdateFrequencyPlottable() const {
    const std::set<StorePath> paths = views::concat(views::keys(CommittedUpdateTimesForPath), views::keys(GestureUpdateTimesForPath)) | to<std::set>;
    if (paths.empty()) return {};

    const bool has_gesture = !GestureUpdateTimesForPath.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    Count i = 0;
    for (const auto &path : paths) values[i++] = CommittedUpdateTimesForPath.contains(path) ? CommittedUpdateTimesForPath.at(path).size() : 0;
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture)
        for (const auto &path : paths) values[i++] = GestureUpdateTimesForPath.contains(path) ? GestureUpdateTimesForPath.at(path).size() : 0;

    const auto labels = paths | transform([](const string &path) {
                            // Convert `string` to char array, removing first character of the path, which is a '/'.
                            char *label = new char[path.size()];
                            std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
                            return label;
                        }) |
        to<vector<const char *>>;

    return {labels, values};
}

void StoreHistory::SetIndex(Count new_index) {
    // If we're mid-gesture, revert the current gesture before navigating to the requested history index.
    if (!ActiveGesture.empty()) {
        ActiveGesture.clear();
        GestureUpdateTimesForPath.clear();
    }
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    const Count old_index = Index;
    Index = new_index;

    const auto direction = new_index > old_index ? Forward : Reverse;
    auto i = int(old_index);
    while (i != int(new_index)) {
        const int history_index = direction == Reverse ? --i : i++;
        const Count record_index = history_index == -1 ? Index : history_index;
        const auto &segment_patch = CreatePatch(record_index + 1);
        const auto &gesture_time = Records[record_index + 1].Gesture.back().second;
        for (const auto &[partial_path, op] : segment_patch.Ops) {
            const auto &path = segment_patch.BasePath / partial_path;
            if (direction == Forward) {
                CommittedUpdateTimesForPath[path].emplace_back(gesture_time);
            } else if (CommittedUpdateTimesForPath.contains(path)) {
                auto &update_times = CommittedUpdateTimesForPath.at(path);
                update_times.pop_back();
                if (update_times.empty()) CommittedUpdateTimesForPath.erase(path);
            }
        }
    }
    GestureUpdateTimesForPath.clear();
}

namespace Action {
bool Undo::Allowed() { return History.CanUndo(); }
bool Redo::Allowed() { return History.CanRedo(); }
bool SaveProject::Allowed() { return !History.Empty(); }
bool SaveDefaultProject::Allowed() { return !History.Empty(); }
} // namespace Action

StoreHistory History{};
