#pragma once

// Forward declaration of store types.

#include <immer/memory_policy.hpp>

#include "StoreTypes.h"

namespace immer {
template<typename K, typename T, typename Hash, typename Equal, typename MemoryPolicy, std::uint32_t B>
class map;
} // namespace immer

const auto immer_default_bits = 5;
using Store = immer::map<StorePath, Primitive, StorePathHash, std::equal_to<StorePath>, immer::default_memory_policy, immer_default_bits>;
