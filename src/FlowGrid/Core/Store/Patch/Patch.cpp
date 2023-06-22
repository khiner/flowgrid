#include "Patch.h"

#include <filesystem>

std::string to_string(PatchOp::Type patch_op_type) {
    switch (patch_op_type) {
        case PatchOp::Type::Add: return "Add";
        case PatchOp::Type::Remove: return "Remove";
        case PatchOp::Type::Replace: return "Replace";
    }
}

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    static constexpr auto AddOp = PatchOp::Type::Add;
    static constexpr auto RemoveOp = PatchOp::Type::Remove;
    static constexpr auto ReplaceOp = PatchOp::Type::Replace;

    PatchOps merged = a;
    for (const auto &[path, op] : b) {
        if (merged.contains(path)) {
            const auto &old_op = merged.at(path);
            // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
            // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
            // For example, if the first patch removes a path, and the second one adds the same path,
            // we can't know from only looking at the pair whether the added value was the same as it was before the remove
            // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
            if (old_op.Op == AddOp) {
                if (op.Op == RemoveOp || ((op.Op == AddOp || op.Op == ReplaceOp) && old_op.Value == op.Value)) merged.erase(path); // Cancel out
                else merged[path] = {AddOp, op.Value, {}};
            } else if (old_op.Op == RemoveOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) {
                    if (old_op.Value == op.Value) merged.erase(path); // Cancel out
                    else merged[path] = {ReplaceOp, op.Value, old_op.Old};
                } else {
                    merged[path] = {RemoveOp, {}, old_op.Old};
                }
            } else if (old_op.Op == ReplaceOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) merged[path] = {ReplaceOp, op.Value, old_op.Old};
                else merged[path] = {RemoveOp, {}, old_op.Old};
            }
        } else {
            merged[path] = op;
        }
    }

    return merged;
}

bool Patch::IsPrefixOfAnyPath(const StorePath &path) const noexcept {
    return std::ranges::any_of(GetPaths(), [&path](const StorePath &candidate_path) {
        const auto &[first_mismatched_path_it, _] = std::mismatch(path.begin(), path.end(), candidate_path.begin(), candidate_path.end());
        return first_mismatched_path_it == path.end();
    });
}
