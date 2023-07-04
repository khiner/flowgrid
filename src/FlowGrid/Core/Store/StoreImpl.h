#pragma once

#include <unordered_map>

#include "immer/map.hpp"
#include "immer/set.hpp"

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"
#include "IdPair.h"

struct TransientStoreImpl;

struct StoreImpl {
    using PrimitiveMap = immer::map<StorePath, Primitive, PathHash>;
    using IdPairs = immer::set<IdPair, IdPairHash>;
    using IdPairsMap = std::unordered_map<StorePath, IdPairs, PathHash>;

    PrimitiveMap PrimitiveByPath;
    IdPairsMap IdPairsByPath;

    TransientStoreImpl Transient() const;
};
