#include "Patch.h"

#include <algorithm>

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    static constexpr auto AddOp = PatchOpType::Add, RemoveOp = PatchOpType::Remove, ReplaceOp = PatchOpType::Replace;

    PatchOps merged = a;
    for (const auto &[id, ops] : b) {
        if (!merged.contains(id)) {
            merged[id] = ops;
            continue;
        }

        auto &old_ops = merged.at(id);
        if (old_ops.size() > 1) {
            for (const auto &op : ops) old_ops.push_back(op);
            continue;
        }

        const auto &old_op = old_ops.front();
        const auto &op = ops.front();
        // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
        // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
        // For example, if the first patch removes a component, and the second one adds the same component,
        // we can't know from only looking at the pair whether the added value was the same as it was before the remove
        // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
        if (old_op.Op == AddOp) {
            if (op.Op == RemoveOp || ((op.Op == AddOp || op.Op == ReplaceOp) && old_op.Value == op.Value)) merged.erase(id); // Cancel out
            else merged[id] = {{AddOp, op.Value, {}}};
        } else if (old_op.Op == RemoveOp) {
            if (op.Op == AddOp || op.Op == ReplaceOp) {
                if (old_op.Value == op.Value) merged.erase(id); // Cancel out
                else merged[id] = {{ReplaceOp, op.Value, old_op.Old}};
            } else {
                merged[id] = {{RemoveOp, {}, old_op.Old}};
            }
        } else if (old_op.Op == ReplaceOp) {
            if (op.Op == AddOp || op.Op == ReplaceOp) merged[id] = {{ReplaceOp, op.Value, old_op.Old}};
            else merged[id] = {{RemoveOp, {}, old_op.Old}};
        }
    }

    return merged;
}
