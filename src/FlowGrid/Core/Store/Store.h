#include "TypedStore.h"

#include "Core/Action/Actionable.h"
#include "IdPairs.h"
#include "StoreAction.h"

#include "immer/set.hpp"
#include "immer/vector.hpp"

struct Store : TypedStore<bool, u32, s32, float, std::string, IdPairs, immer::set<u32>, immer::vector<u32>>,
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
        return ContainsPrimitive(path) || ContainsPrimitive(path / "0") || Count<IdPairs>(path);
    }

    void ApplyPatch(const Patch &patch) const {
        for (const auto &[partial_path, op] : patch.Ops) {
            const auto path = patch.BasePath / partial_path;
            std::visit([this, &path, &op](auto &&v) {
                if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) Set(path, std::move(v));
                else if (op.Op == PatchOpType::Remove) ErasePrimitive(path);
            },
                       *op.Value);
        }
    }

    void ErasePrimitive(const StorePath &path) const {
        if (Count<bool>(path)) Erase<bool>(path);
        else if (Count<u32>(path)) Erase<u32>(path);
        else if (Count<s32>(path)) Erase<s32>(path);
        else if (Count<float>(path)) Erase<float>(path);
        else if (Count<std::string>(path)) Erase<std::string>(path);
    }

    bool ContainsPrimitive(const StorePath &path) const {
        return Count<bool>(path) || Count<u32>(path) || Count<s32>(path) || Count<float>(path) || Count<std::string>(path);
    }
};
