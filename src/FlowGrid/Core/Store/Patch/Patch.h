#pragma once

#include <ranges>
#include <unordered_map>

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"
#include "PatchOp.h"

using PatchOps = std::unordered_map<StorePath, PatchOp, PathHash>;
PatchOps Merge(const PatchOps &a, const PatchOps &b);

struct Patch {
    PatchOps Ops;
    StorePath BasePath{RootPath};

    // Returns a view.
    inline auto GetPaths() const noexcept {
        return Ops | std::views::keys | std::views::transform([this](const auto &partial_path) { return BasePath / partial_path; });
    }

    bool Empty() const noexcept { return Ops.empty(); }
    bool IsPrefixOfAnyPath(const StorePath &) const noexcept;
};
