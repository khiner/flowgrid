#pragma once

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "StoreTypes.h"

struct Store;

struct TransientStore {
    using Map = immer::map_transient<StorePath, Primitive, PathHash>;
    Map PrimitiveForPath;

    Store Persistent();
};
