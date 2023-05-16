#pragma once

// Forward declaration of store types.

#include <immer/memory_policy.hpp>

#include "StoreTypes.h"

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
