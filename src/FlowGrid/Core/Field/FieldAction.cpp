#include "FieldAction.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

namespace Action {
std::variant<Primitive::Set, bool> Primitive::Set::Merge(const Primitive::Set &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<Primitive::SetMany, bool> Primitive::SetMany::Merge(const Primitive::SetMany &other) const {
    return Primitive::SetMany{ranges::views::concat(values, other.values) | ranges::to<std::vector>};
}
std::variant<Vector::Set, bool> Vector::Set::Merge(const Vector::Set &other) const {
    if (path == other.path) return other;
    return false;
}
std::variant<Matrix::Set, bool> Matrix::Set::Merge(const Matrix::Set &other) const {
    if (path == other.path) return other;
    return false;
}
} // namespace Action
