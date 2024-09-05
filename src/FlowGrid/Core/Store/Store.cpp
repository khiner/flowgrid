#include "Store.h"

void Store::Apply(const ActionType &action) const {
    std::visit(
        [this](const Action::Store::ApplyPatch &a) {
            for (const auto &[id, ops] : a.patch.Ops) {
                for (const auto &op : ops) {
                    if (op.Op == PatchOpType::PopBack) {
                        std::visit(
                            [&](auto &&v) {
                                const auto vec = Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id);
                                Set(id, vec.take(vec.size() - 1));
                            },
                            *op.Old
                        );
                    } else if (op.Op == PatchOpType::Remove) {
                        std::visit([&](auto &&v) { Erase<std::decay_t<decltype(v)>>(id); }, *op.Old);
                    } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                        std::visit([&](auto &&v) { Set(id, std::move(v)); }, *op.Value);
                    } else if (op.Op == PatchOpType::PushBack) {
                        std::visit([&](auto &&v) { Set(id, Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).push_back(std::move(v))); }, *op.Value);
                    } else if (op.Op == PatchOpType::Set) {
                        std::visit([&](auto &&v) { Set(id, Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).set(*op.Index, std::move(v))); }, *op.Value);
                    } else {
                        // `set` ops - currently, u32 is the only set value type.
                        std::visit(
                            Match{
                                [&](u32 v) {
                                    if (op.Op == PatchOpType::Insert) Set(id, Get<immer::set<decltype(v)>>(id).insert(v));
                                    else if (op.Op == PatchOpType::Erase) Set(id, Get<immer::set<decltype(v)>>(id).erase(v));
                                },
                                [](auto &&) {},
                            },
                            *op.Value
                        );
                    }
                }
            }
        },
        action
    );
}
