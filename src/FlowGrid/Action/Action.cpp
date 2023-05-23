#include "Action.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

using namespace std::string_literals;
using namespace Action;
using ranges::to;
namespace views = ranges::views;

std::variant<OpenFaustFile, bool> OpenFaustFile::Merge(const OpenFaustFile &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetValue, bool> SetValue::Merge(const SetValue &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetValues, bool> SetValues::Merge(const SetValues &other) const {
    return SetValues{views::concat(values, other.values) | to<std::vector>};
}
std::variant<SetVector, bool> SetVector::Merge(const SetVector &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetMatrix, bool> SetMatrix::Merge(const SetMatrix &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<ApplyPatch, bool> ApplyPatch::Merge(const ApplyPatch &other) const {
    // Keep patch actions affecting different base state-paths separate,
    // since actions affecting different state bases are likely semantically different.
    const auto &ops = ::Merge(patch.Ops, other.patch.Ops);
    if (ops.empty()) return true;
    if (patch.BasePath == other.patch.BasePath) return ApplyPatch{ops, other.patch.BasePath};
    return false;
}

namespace Action {
Gesture MergeGesture(const Gesture &gesture) {
    Gesture merged_gesture; // Mutable return value

    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const StatefulActionMoment> active;
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
            [&](const StatefulAction &merged_action) {
                active.emplace(merged_action, b.second); // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
            },
        );
    }
    if (active) merged_gesture.emplace_back(*active);

    return merged_gesture;
}
} // namespace Action

namespace nlohmann {
void to_json(json &j, const Action::StatefulAction &action) {
    action.to_json(j);
}
void from_json(const json &j, Action::StatefulAction &action) {
    Action::StatefulAction::from_json(j, action);
}
} // namespace nlohmann
