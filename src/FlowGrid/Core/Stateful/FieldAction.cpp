#include "FieldAction.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

namespace Action {
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
} // namespace Action
