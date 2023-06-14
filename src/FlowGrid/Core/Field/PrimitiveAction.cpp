#include "PrimitiveAction.h"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>

namespace Action {
std::variant<Primitive::Set, bool> Primitive::Set::Merge(const Primitive::Set &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<Primitive::SetMany, bool> Primitive::SetMany::Merge(const Primitive::SetMany &other) const {
    return Primitive::SetMany{ranges::views::concat(values, other.values) | ranges::to<std::vector>};
}
} // namespace Action
