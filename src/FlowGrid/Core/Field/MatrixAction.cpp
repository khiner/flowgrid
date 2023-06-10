#include "MatrixAction.h"

namespace Action {
std::variant<Matrix::Set, bool> Matrix::Set::Merge(const Matrix::Set &other) const {
    if (path == other.path) return other;
    return false;
}
} // namespace Action
