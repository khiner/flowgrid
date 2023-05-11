#pragma once

#include <unordered_map>

#include "../Helper/Time.h"
#include "../Primitive.h"

namespace fs = std::filesystem;

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

using PatchOps = std::unordered_map<StorePath, PatchOp, StorePathHash>;

static constexpr auto AddOp = PatchOp::Type::Add;
static constexpr auto RemoveOp = PatchOp::Type::Remove;
static constexpr auto ReplaceOp = PatchOp::Type::Replace;

struct Patch {
    PatchOps Ops;
    StorePath BasePath{RootPath};

    bool Empty() const noexcept { return Ops.empty(); }
};

struct StatePatch {
    Patch Patch{};
    TimePoint Time{};
};

string to_string(PatchOp::Type);

#include <immer/memory_policy.hpp>

namespace immer {
template<typename K, typename T, typename Hash, typename Equal, typename MemoryPolicy, std::uint32_t B>
class map;

template<typename K, typename T, typename Hash, typename Equal, typename MemoryPolicy, std::uint32_t B>
class map_transient;
} // namespace immer

const auto immer_default_bits = 5;
using Store = immer::map<StorePath, Primitive, StorePathHash, std::equal_to<StorePath>, immer::default_memory_policy, immer_default_bits>;
using TransientStore = immer::map_transient<StorePath, Primitive, StorePathHash, std::equal_to<StorePath>, immer::default_memory_policy, immer_default_bits>;

extern TransientStore InitStore; // Used in `StateMember` constructors to initialize the store.
extern const Store &AppStore; // Global read-only accessor for the canonical application store instance.
