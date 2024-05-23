#pragma once

#include "TypedStore.h"

#include "Core/Action/Actionable.h"
#include "StoreAction.h"

// Specialized `ValueTypes`
#include "IdPairs.h"

#include "immer/set.hpp"
#include "immer/vector.hpp"

struct Store : TypedStore<
                   bool, u32, s32, float, std::string, IdPairs, immer::set<u32>,
                   immer::vector<bool>, immer::vector<s32>, immer::vector<u32>, immer::vector<float>, immer::vector<std::string>>,
               Actionable<Action::Store::Any> {
    bool CanApply(const ActionType &) const override { return true; }
    void ApplyPatch(const Patch &) const;
    void Apply(const ActionType &action) const override {
        std::visit([this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); }, action);
    }

    // Set operations
    template<typename ValueType> void Insert(ID set_id, const ValueType &value) const {
        if (!Count<immer::set<ValueType>>(set_id)) Set(set_id, immer::set<ValueType>{});
        Set(set_id, Get<immer::set<ValueType>>(set_id).insert(value));
    }
    template<typename ValueType> void SetErase(ID set_id, const ValueType &value) const {
        if (Count<immer::set<ValueType>>(set_id)) Set(set_id, Get<immer::set<ValueType>>(set_id).erase(value));
    }

    // Vector operations
    template<typename ValueType> void VectorSet(ID vec_id, size_t i, const ValueType &value) const {
        if (Count<immer::vector<ValueType>>(vec_id)) {
            Set(vec_id, Get<immer::vector<ValueType>>(vec_id).set(i, value));
        }
    }
    template<typename ValueType> void PushBack(ID vec_id, const ValueType &value) const {
        if (!Count<immer::vector<ValueType>>(vec_id)) Set(vec_id, immer::vector<ValueType>{});
        Set(vec_id, Get<immer::vector<ValueType>>(vec_id).push_back(value));
    }
    template<typename ValueType> void PopBack(ID vec_id) const {
        if (Count<immer::vector<ValueType>>(vec_id)) {
            auto vec = Get<immer::vector<ValueType>>(vec_id);
            Set(vec_id, vec.take(vec.size() - 1));
        }
    }
};
