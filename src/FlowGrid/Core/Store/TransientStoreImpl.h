#pragma once

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Core/Primitive.h"
#include "Helper/Path.h"

struct Store;

struct TransientStore {
    using Map = immer::map_transient<StorePath, Primitive, PathHash>;
    Map PrimitiveForPath;

    Store Persistent();
};
