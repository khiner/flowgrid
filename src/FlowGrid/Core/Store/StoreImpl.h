#pragma once

#include "immer/map.hpp"

#include "Core/Primitive.h"
#include "Helper/Path.h"

struct TransientStore;

struct Store {
    using Map = immer::map<StorePath, Primitive, PathHash>;
    Map PrimitiveForPath;

    TransientStore Transient() const;
};
