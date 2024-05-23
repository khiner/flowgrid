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