#include "Store.h"

void Store::ApplyPatch(const Patch &patch) const {
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

template<typename ValueType> void Store::Insert(ID set_id, const ValueType &value) const {
    Set(set_id, Get<immer::set<ValueType>>(set_id).insert(value));
}
template<typename ValueType> void Store::SetErase(ID set_id, const ValueType &value) const {
    Set(set_id, Get<immer::set<ValueType>>(set_id).erase(value));
}

template<typename ValueType> void Store::VectorSet(ID vec_id, size_t i, const ValueType &value) const {
    Set(vec_id, Get<immer::flex_vector<ValueType>>(vec_id).set(i, value));
}
template<typename ValueType> void Store::PushBack(ID vec_id, const ValueType &value) const {
    Set(vec_id, Get<immer::flex_vector<ValueType>>(vec_id).push_back(value));
}
template<typename ValueType> void Store::PopBack(ID vec_id) const {
    auto vec = Get<immer::flex_vector<ValueType>>(vec_id);
    Set(vec_id, vec.take(vec.size() - 1));
}