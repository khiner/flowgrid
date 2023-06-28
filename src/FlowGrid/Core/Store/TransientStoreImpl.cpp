#include "TransientStoreImpl.h"

#include <range/v3/range/conversion.hpp>

#include <ranges>

#include "StoreImpl.h"

Store TransientStore::Persistent() {
    Store::IdPairsMap PersistentIdPairsByPath;
    for (auto &[path, id_pairs] : IdPairsByPath) PersistentIdPairsByPath.emplace(path, id_pairs.persistent());

    return {PrimitiveByPath.persistent(), PersistentIdPairsByPath};
}
