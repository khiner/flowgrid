#pragma once

// Store-related types like path/entry/patch.
// Basically all store-related types except the actual `Store` and `TransientStore` types, which are in `StoreFwd.h`.

#include <unordered_map>
#include <vector>

#include "Helper/Time.h"
#include "Core/Primitive.h"

#include <__filesystem/path.h>

namespace fs = std::filesystem;
using StorePath = std::filesystem::path;
inline static const StorePath RootPath{"/"};

using StoreEntry = std::pair<StorePath, Primitive>;
using StoreEntries = std::vector<StoreEntry>;

struct StorePathHash {
    auto operator()(const StorePath &p) const noexcept { return fs::hash_value(p); }
};

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

using PatchOps = std::unordered_map<StorePath, PatchOp, StorePathHash>;
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
