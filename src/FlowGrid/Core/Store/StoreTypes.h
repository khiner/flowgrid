#pragma once

// Store-related types like path/entry/patch.
// Basically all store-related types except the actual `Store` type.

#include <unordered_map>
#include <vector>

#include "Core/Primitive.h"
#include "Helper/Time.h"
#include "Helper/Path.h"


using StorePath = fs::path;
using StoreEntry = std::pair<StorePath, Primitive>;
using StoreEntries = std::vector<StoreEntry>;

inline static const StorePath RootPath{"/"};

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

struct StatePatch {
    Patch Patch{};
    TimePoint Time{};
};
