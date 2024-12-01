#pragma once

#include "StoreBase.h"

#include "Core/Scalar.h"
#include "Core/TextEditor/TextBufferData.h"
#include "IdPairs.h"

// Define persistent and transient maps types without needing to repeat the value types.
template<typename... Ts> struct StoreTypesBase {
    using Persistent = StoreMaps<Ts...>;
    using Transient = TransientStoreMaps<Ts...>;
};

using StoreTypes = StoreTypesBase<
    bool, u32, s32, float, std::string, IdPairs, TextBufferData,
    immer::set<u32>, immer::flex_vector<bool>, immer::flex_vector<s32>,
    immer::flex_vector<u32>, immer::flex_vector<float>, immer::flex_vector<std::string>>;

// Support forward declaration of store types.
struct PersistentStore : StoreTypes::Persistent {
    PersistentStore() = default;
    PersistentStore(StoreTypes::Persistent &&persistent) : StoreTypes::Persistent(std::move(persistent)) {}
};
struct TransientStore : StoreTypes::Transient {
    TransientStore() = default;
    TransientStore(StoreTypes::Transient &&transient) : StoreTypes::Transient(std::move(transient)) {}
};
