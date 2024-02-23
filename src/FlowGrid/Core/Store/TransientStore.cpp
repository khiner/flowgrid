#include "TransientStore.h"

#include "Store.h"

Store TransientStore::Persistent() {
    return {PrimitiveByPath.persistent(), IdPairsByPath.persistent()};
}
