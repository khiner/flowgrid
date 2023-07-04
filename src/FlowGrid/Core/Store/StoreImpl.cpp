#include "StoreImpl.h"

#include "TransientStoreImpl.h"

TransientStoreImpl StoreImpl::Transient() const {
    TransientStoreImpl::IdPairsMap TransientIdPairsByPath;
    for (auto &[path, id_pairs] : IdPairsByPath) TransientIdPairsByPath.emplace(path, id_pairs.transient());

    return {PrimitiveByPath.transient(), TransientIdPairsByPath};
}
