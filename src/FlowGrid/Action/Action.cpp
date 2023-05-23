#include "Action.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

namespace Action {
std::variant<OpenFaustFile, bool> OpenFaustFile::Merge(const OpenFaustFile &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetValue, bool> SetValue::Merge(const SetValue &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetValues, bool> SetValues::Merge(const SetValues &other) const {
    return SetValues{ranges::views::concat(values, other.values) | ranges::to<std::vector>};
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
} // namespace Action

namespace nlohmann {
void to_json(json &j, const Action::StatefulAction &action) {
    action.to_json(j);
}
void from_json(const json &j, Action::StatefulAction &action) {
    Action::StatefulAction::from_json(j, action);
}
} // namespace nlohmann
