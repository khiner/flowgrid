#pragma once

#include "immer/map.hpp"

#include "StoreTypes.h"

struct TransientStore;

struct Store {
    using Map = immer::map<StorePath, Primitive, PathHash>;
    Map PrimitiveForPath;

    TransientStore Transient() const;
};
