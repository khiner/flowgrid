#include "StoreImpl.h"

#include "TransientStoreImpl.h"

TransientStore Store::Transient() const {
    TransientStore::IdPairsMap TransientIdPairsByPath;
    for (auto &[path, id_pairs] : IdPairsByPath) TransientIdPairsByPath.emplace(path, id_pairs.transient());

    return {PrimitiveByPath.transient(), TransientIdPairsByPath};
}
