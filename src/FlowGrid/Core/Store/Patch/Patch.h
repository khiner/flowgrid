#pragma once

#include <unordered_map>

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"
#include "Helper/Time.h"

struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    PatchOp::Type Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};

std::string to_string(PatchOp::Type);

using PatchOps = std::unordered_map<StorePath, PatchOp, PathHash>;
PatchOps Merge(const PatchOps &a, const PatchOps &b);

struct Patch {
    PatchOps Ops;
    StorePath BasePath{RootPath};

    bool Empty() const noexcept { return Ops.empty(); }
};
