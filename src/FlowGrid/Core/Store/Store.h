#pragma once

#include "TypedStore.h"

#include "Core/Action/Actionable.h"
#include "StoreAction.h"

// Specialized `ValueTypes`
#include "IdPairs.h"
#include "Project/TextEditor/TextBufferData.h"

struct Store : TypedStore<
                   bool, u32, s32, float, std::string, IdPairs, TextBufferData, immer::set<u32>,
                   immer::flex_vector<bool>, immer::flex_vector<s32>, immer::flex_vector<u32>, immer::flex_vector<float>, immer::flex_vector<std::string>>,
               Actionable<Action::Store::Any> {
    bool CanApply(const ActionType &) const override { return true; }
    void ApplyPatch(const Patch &) const;
    void Apply(const ActionType &action) const override {
        std::visit([this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); }, action);
    }

    // Set operations
    template<typename ValueType> void Insert(ID set_id, const ValueType &) const;
    template<typename ValueType> void SetErase(ID set_id, const ValueType &) const;

    // Vector operations
    template<typename ValueType> void VectorSet(ID vec_id, size_t i, const ValueType &) const;
    template<typename ValueType> void PushBack(ID vec_id, const ValueType &) const;
    template<typename ValueType> void PopBack(ID vec_id) const;
};
