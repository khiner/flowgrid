#include "FieldAction.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

namespace Action {
std::variant<SetPrimitive, bool> SetPrimitive::Merge(const SetPrimitive &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<SetPrimitives, bool> SetPrimitives::Merge(const SetPrimitives &other) const {
    return SetPrimitives{ranges::views::concat(values, other.values) | ranges::to<std::vector>};
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
