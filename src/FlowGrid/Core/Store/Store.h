#include "TypedStore.h"

#include "Core/Action/Actionable.h"
#include "IdPairs.h"
#include "StoreAction.h"

#include "immer/set.hpp"
#include "immer/vector.hpp"

// xxx should inherit from `TypedStore`.
struct Store : TypedStore<bool, u32, s32, float, std::string, IdPairs, immer::set<u32>, immer::vector<u32>>,
               Actionable<Action::Store::Any> {
    using TypedStore::TypedStore;
    // Implement copy constructor and assignment operator to avoid the default ones.
    Store(const Store &other) : TypedStore(other) {}
    Store &operator=(const Store &other) {
        TypedStore::operator=(other);
        return *this;
    }

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

    bool ContainsPrimitive(const StorePath &path) const {
        return Count<bool>(path) || Count<u32>(path) || Count<s32>(path) || Count<float>(path) || Count<std::string>(path);
    }
    void ErasePrimitive(const StorePath &path) const {
        Erase<bool>(path);
        Erase<u32>(path);
        Erase<s32>(path);
        Erase<float>(path);
        Erase<std::string>(path);
    }
};
