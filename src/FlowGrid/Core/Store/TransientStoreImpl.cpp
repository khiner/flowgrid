#include "TransientStoreImpl.h"

#include "StoreImpl.h"

Store TransientStore::Persistent() { return {PrimitiveForPath.persistent()}; }
