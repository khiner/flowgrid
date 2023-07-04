#include "TransientStoreImpl.h"

#include <range/v3/range/conversion.hpp>

#include <ranges>

#include "StoreImpl.h"

StoreImpl TransientStoreImpl::Persistent() {
    StoreImpl::IdPairsMap PersistentIdPairsByPath;
    for (auto &[path, id_pairs] : IdPairsByPath) PersistentIdPairsByPath.emplace(path, id_pairs.persistent());

    return {PrimitiveByPath.persistent(), PersistentIdPairsByPath};
}
