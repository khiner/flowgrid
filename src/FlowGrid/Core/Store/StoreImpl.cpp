#include "StoreImpl.h"

#include "TransientStoreImpl.h"

TransientStore Store::Transient() const { return {PrimitiveForPath.transient()}; }
