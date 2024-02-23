#pragma once

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "immer/set.hpp"
#include "immer/set_transient.hpp"

#include "Core/Primitive/PrimitiveVariant.h"
#include "Helper/Path.h"
#include "IdPairs.h"

struct Store;

struct TransientStore {
    using PrimitiveMap = immer::map_transient<StorePath, PrimitiveVariant, PathHash>;
    using IdPairs = immer::set_transient<IdPair, IdPairHash>;
    using IdPairsMap = immer::map_transient<StorePath, immer::set<IdPair, IdPairHash>, PathHash>;

    PrimitiveMap PrimitiveByPath;
    IdPairsMap IdPairsByPath;

    Store Persistent();
};
