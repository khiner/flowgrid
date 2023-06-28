#include "Store.h"

#include <set>

#include "immer/algorithm.hpp"

#include "Core/Primitive/PrimitiveJson.h"
#include "StoreImpl.h"
#include "TransientStoreImpl.h"

namespace store {
void ApplyPatch(const Patch &patch) {
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        if (op.Op == PatchOp::Type::Add || op.Op == PatchOp::Type::Replace) Set(path, *op.Value);
        else if (op.Op == PatchOp::Type::Remove) Erase(path);
    }
}

void ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
    );
}

Store AppStore{};

static const std::string IdPairsPrefix = "id_pairs::";

using namespace nlohmann;

json GetJson(const Store &store) {
    // TODO serialize using the concrete primitive type and avoid the ambiguous Primitive JSON conversion.
    //   - This will be easier after separating container storage, since each `PrimitiveByPath` entry will correspond to a single `PrimitiveField`.
    json j;
    for (const auto &[path, primitive] : store.PrimitiveByPath) {
        j[json::json_pointer(path.string())] = primitive;
    }
    for (const auto &[path, id_pairs] : store.IdPairsByPath) {
        j[json::json_pointer(path.string())] = std::format("{}{}", IdPairsPrefix, json(id_pairs).dump());
    }
    return j;
}

Store JsonToStore(const json &j) {
    const auto &flattened = j.flatten();
    std::vector<std::pair<StorePath, Primitive>> entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value] : flattened.items()) entries[item_index++] = {StorePath(key), Primitive(value)};

    TransientStore transient;
    for (const auto &[path, value] : entries) {
        if (std::holds_alternative<std::string>(value) && std::get<std::string>(value).starts_with(IdPairsPrefix)) {
            std::set<IdPair> id_pairs = json::parse(std::get<std::string>(value).substr(IdPairsPrefix.size()));
            for (const auto &id_pair : id_pairs) {
                transient.IdPairsByPath[path].insert(id_pair);
            }
        } else {
            transient.PrimitiveByPath.set(path, value);
        }
    }

    return transient.Persistent();
}

const Store &Get() { return AppStore; }
json GetJson() { return GetJson(AppStore); }

TransientStore Transient{};
bool IsTransient = true;

void BeginTransient() {
    if (IsTransient) return;

    Transient = AppStore.Transient();
    IsTransient = true;
}

// End transient mode and return the new persistent store.
// Not exposed publicly (use `Commit` instead).
const Store EndTransient() {
    if (!IsTransient) return AppStore;

    const Store new_store = Transient.Persistent();
    Transient = {};
    IsTransient = false;

    return new_store;
}

void Commit() {
    AppStore = EndTransient();
}

Patch CheckedSet(const Store &store) {
    const auto &patch = CreatePatch(store);
    if (patch.Empty()) return {};

    AppStore = store;
    return patch;
}

Patch SetJson(const json &j) {
    Transient = {};
    IsTransient = false;
    return CheckedSet(JsonToStore(j));
}

Patch CheckedCommit() { return CheckedSet(EndTransient()); }

Store GetPersistent() { return Transient.Persistent(); }

Primitive Get(const StorePath &path) { return IsTransient ? Transient.PrimitiveByPath.at(path) : AppStore.PrimitiveByPath.at(path); }
void Set(const StorePath &path, const Primitive &value) {
    if (IsTransient) {
        Transient.PrimitiveByPath.set(path, value);
    } else {
        auto _ = AppStore.PrimitiveByPath.set(path, value);
    }
}
void Erase(const StorePath &path) {
    if (IsTransient) {
        Transient.PrimitiveByPath.erase(path);
    } else {
        auto _ = AppStore.PrimitiveByPath.erase(path);
    }
}

void AddIdPair(const StorePath &path, const IdPair &value) {
    if (IsTransient) {
        if (Transient.IdPairsByPath.contains(path)) {
            Transient.IdPairsByPath[path].insert(value);
        } else {
            Transient.IdPairsByPath[path] = {};
            Transient.IdPairsByPath[path].insert(value);
        }
    } else {
        if (AppStore.IdPairsByPath.contains(path)) {
            auto _ = AppStore.IdPairsByPath[path].insert(value);
        } else {
            AppStore.IdPairsByPath[path] = {value};
        }
    }
}
void EraseIdPair(const StorePath &path, const IdPair &value) {
    if (IsTransient) {
        Transient.IdPairsByPath[path].erase(value);
    } else {
        auto _ = AppStore.IdPairsByPath[path].erase(value);
    }
}
bool HasIdPair(const StorePath &path, const IdPair &value) {
    if (IsTransient) {
        if (!Transient.IdPairsByPath.contains(path)) return false;
        return Transient.IdPairsByPath[path].count(value) > 0;
    } else {
        if (!AppStore.IdPairsByPath.contains(path)) return false;
        return AppStore.IdPairsByPath[path].count(value) > 0;
    }
}

Count CountAt(const StorePath &path) { return IsTransient ? Transient.PrimitiveByPath.count(path) : AppStore.PrimitiveByPath.count(path); }

Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path) {
    PatchOps ops{};

    diff(
        before.PrimitiveByPath,
        after.PrimitiveByPath,
        [&](auto const &added) {
            ops[added.first.lexically_relative(base_path)] = {PatchOp::Type::Add, added.second, {}};
        },
        [&](auto const &removed) {
            ops[removed.first.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, removed.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(base_path)] = {PatchOp::Type::Replace, new_element.second, old_element.second};
        }
    );

    // Added IdPair sets:
    for (const auto &[id_pairs_path, id_pairs] : after.IdPairsByPath) {
        if (!before.IdPairsByPath.contains(id_pairs_path)) {
            for (const auto &id_pair : id_pairs) {
                ops[id_pairs_path.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(id_pair), {}};
            }
        }
    }
    // Removed IdPair sets:
    for (const auto &[id_pairs_path, id_pairs] : before.IdPairsByPath) {
        if (!after.IdPairsByPath.contains(id_pairs_path)) {
            for (const auto &id_pair : id_pairs) {
                ops[id_pairs_path.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(id_pair)};
            }
        }
    }
    // Changed IdPair sets:
    for (const auto &id_pair_path : std::views::keys(before.IdPairsByPath)) {
        if (!after.IdPairsByPath.contains(id_pair_path)) continue;
        diff(
            before.IdPairsByPath.at(id_pair_path),
            after.IdPairsByPath.at(id_pair_path),
            [&](const auto &added) {
                ops[id_pair_path.lexically_relative(base_path)] = {PatchOp::Type::Add, SerializeIdPair(added), {}};
            },
            [&](const auto &removed) {
                ops[id_pair_path.lexically_relative(base_path)] = {PatchOp::Type::Remove, {}, SerializeIdPair(removed)};
            },
            // Change callback required but never called for `immer::set`.
            [&](const auto &, const auto &) {}
        );
    }

    return {ops, base_path};
}

Patch CreatePatch(const Store &store, const StorePath &base_path) { return CreatePatch(AppStore, store, base_path); }
Patch CreatePatch(const StorePath &base_path) { return CreatePatch(AppStore, EndTransient(), base_path); }
} // namespace store
