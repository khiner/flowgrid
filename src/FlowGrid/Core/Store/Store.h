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

    void ApplyPatch(const Patch &patch) const {
        for (const auto &[id, ops] : patch.Ops) {
            for (const auto &op : ops) {
                if (op.Op == PatchOpType::PopBack) {
                    std::visit([&](auto &&v) { PopBack<std::decay_t<decltype(v)>>(id); }, *op.Old);
                } else if (op.Op == PatchOpType::Remove) {
                    std::visit([&](auto &&v) { Erase<std::decay_t<decltype(v)>>(id); }, *op.Old);
                } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                    std::visit([&](auto &&v) { Set(id, std::move(v)); }, *op.Value);
                } else if (op.Op == PatchOpType::PushBack) {
                    std::visit([&](auto &&v) { PushBack(id, std::move(v)); }, *op.Value);
                } else if (op.Op == PatchOpType::Set) {
                    std::visit([&](auto &&v) { VectorSet(id, *op.Index, v); }, *op.Value);
                } else {
                    // `set` ops - currently, u32 is the only set value type.
                    std::visit(
                        Match{
                            [&](u32 v) {
                                if (op.Op == PatchOpType::Insert) Insert(id, v);
                                else if (op.Op == PatchOpType::Erase) SetErase(id, v);
                            },
                            [](auto &&) {},
                        },
                        *op.Value
                    );
                }
            }
        }
    }
};
