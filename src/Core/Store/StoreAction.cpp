#include "StoreAction.h"

namespace Action {
std::variant<Store::ApplyPatch, bool> Store::ApplyPatch::Merge(const Store::ApplyPatch &other) const {
    // Keep patch actions affecting different components separate.
    const auto &ops = ::Merge(patch.Ops, other.patch.Ops);
    if (ops.empty()) return true;
    if (patch.BaseComponentId == other.patch.BaseComponentId) return Store::ApplyPatch{patch.BaseComponentId, ops};
    return false;
}
} // namespace Action
