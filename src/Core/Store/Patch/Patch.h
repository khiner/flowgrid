#pragma once

#include <ranges>
#include <unordered_map>
#include <vector>

#include "Core/ID.h"
#include "PatchOp.h"

using PatchOps = std::unordered_map<ID, std::vector<PatchOp>>;
PatchOps Merge(const PatchOps &a, const PatchOps &b);

struct Patch {
    ID BaseComponentId;
    PatchOps Ops;

    // Returns a view.
    inline auto GetIds() const noexcept { return Ops | std::views::keys; }

    bool Empty() const noexcept { return Ops.empty(); }
};
