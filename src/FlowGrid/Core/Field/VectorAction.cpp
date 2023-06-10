#include "VectorAction.h"

namespace Action {
std::variant<Vector::Set, bool> Vector::Set::Merge(const Vector::Set &other) const {
    if (path == other.path) return other;
    return false;
}
} // namespace Action
