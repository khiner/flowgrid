#include "StoreAction.h"

namespace Action {
std::variant<Store::ApplyPatch, bool> Store::ApplyPatch::Merge(const Store::ApplyPatch &other) const {
    // Keep patch actions affecting different base state-paths separate,
    // since actions affecting different state bases are likely semantically different.
    const auto &ops = ::Merge(patch.Ops, other.patch.Ops);
    if (ops.empty()) return true;
    if (patch.BasePath == other.patch.BasePath) return Store::ApplyPatch{ops, other.patch.BasePath};
    return false;
}
} // namespace Action
