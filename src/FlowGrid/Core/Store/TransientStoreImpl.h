#pragma once

#include <unordered_map>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "immer/set.hpp"
#include "immer/set_transient.hpp"

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"
#include "IdPair.h"

struct Store;

struct TransientStore {
    using PrimitiveMap = immer::map_transient<StorePath, Primitive, PathHash>;
    using IdPairs = immer::set_transient<IdPair, IdPairHash>;
    using IdPairsMap = std::unordered_map<StorePath, IdPairs, PathHash>;

    PrimitiveMap PrimitiveByPath;
    IdPairsMap IdPairsByPath;

    Store Persistent();
};
