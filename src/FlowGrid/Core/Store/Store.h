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
        std::visit(
            Match{
                [this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
            },
            action
        );
    }

    bool Contains(const StorePath &path) const {
        // xxx this is the only place in the store where we use knowledge about vector paths.
        // It should go away after finishing the `PrimitiveVec` refactor to back `PrimitiveVector` with `immer::vector`.
        return ContainsPrimitive(path) || ContainsPrimitive(path / "0") || Count<IdPairs>(path);
    }

    // Set operations
    template<typename ValueType> void Insert(const StorePath &set_path, const ValueType &value) const {
        if (!Count<immer::set<ValueType>>(set_path)) Set(set_path, immer::set<ValueType>{});
        Set(set_path, Get<immer::set<ValueType>>(set_path).insert(value));
    }
    template<typename ValueType> void SetErase(const StorePath &set_path, const ValueType &value) const {
        if (Count<immer::set<ValueType>>(set_path)) Set(set_path, Get<immer::set<ValueType>>(set_path).erase(value));
    }

    // Vector operations
    template<typename ValueType> void VectorSet(const StorePath &vec_path, size_t i, const ValueType &value) const {
        if (Count<immer::vector<ValueType>>(vec_path)) {
            Set(vec_path, Get<immer::vector<ValueType>>(vec_path).set(i, value));
        }
    }
    template<typename ValueType> void PushBack(const StorePath &vec_path, const ValueType &value) const {
        if (!Count<immer::vector<ValueType>>(vec_path)) Set(vec_path, immer::vector<ValueType>{});
        Set(vec_path, Get<immer::vector<ValueType>>(vec_path).push_back(value));
    }
    template<typename ValueType> void PopBack(const StorePath &vec_path) const {
        if (Count<immer::vector<ValueType>>(vec_path)) {
            auto vec = Get<immer::vector<ValueType>>(vec_path);
            Set(vec_path, vec.take(vec.size() - 1));
        }
    }

    void ApplyPatch(const Patch &patch) const {
        for (const auto &[partial_path, ops] : patch.Ops) {
            const auto path = patch.BasePath / partial_path;
            for (const auto &op : ops) {
                if (op.Op == PatchOpType::PopBack) {
                    std::visit([&](auto &&v) { PopBack<std::decay_t<decltype(v)>>(path); }, *op.Old);
                } else if (op.Op == PatchOpType::Remove) {
                    std::visit([&](auto &&v) { Erase<std::decay_t<decltype(v)>>(path); }, *op.Old);
                } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                    std::visit([&](auto &&v) { Set(path, std::move(v)); }, *op.Value);
                } else if (op.Op == PatchOpType::PushBack) {
                    std::visit([&](auto &&v) { PushBack(path, std::move(v)); }, *op.Value);
                } else if (op.Op == PatchOpType::Set) {
                    std::visit([&](auto &&v) { VectorSet(path.parent_path(), std::stoul(std::string(path.filename())), v); }, *op.Value);
                } else {
                    // `set` ops - currently, u32 is the only set value type.
                    std::visit(
                        Match{
                            [&](u32 v) {
                                if (op.Op == PatchOpType::Insert) Insert(path, v);
                                else if (op.Op == PatchOpType::Erase) SetErase(path, v);
                            },
                            [&](auto &&) {},
                        },
                        *op.Value
                    );
                }
            }
        }
    }

    bool ContainsPrimitive(const StorePath &path) const {
        return Count<bool>(path) || Count<u32>(path) || Count<s32>(path) || Count<float>(path) || Count<std::string>(path);
    }
};
