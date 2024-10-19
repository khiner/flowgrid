#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/PrimitiveVariant.h"

enum class PatchOpType {
    // Primitive ops
    Add,
    Remove,
    Replace,
    // Vector ops
    PushBack,
    PopBack,
    Set, // Different from `Replace` since the op's path includes the index.
    // Set ops
    Insert,
    Erase,
};

struct PatchOp {
    PatchOpType Op{};
    std::optional<PrimitiveVariant> Value{}; // Present for add/replace
    std::optional<PrimitiveVariant> Old{}; // Present for remove/replace
    std::optional<size_t> Index{}; // Present for vector set
};

inline std::string ToString(PatchOpType type) {
    switch (type) {
        case PatchOpType::Add: return "Add";
        case PatchOpType::Remove: return "Remove";
        case PatchOpType::Replace: return "Replace";
        case PatchOpType::Set: return "Set";
        case PatchOpType::PushBack: return "PushBack";
        case PatchOpType::PopBack: return "PopBack";
        case PatchOpType::Insert: return "Insert";
        case PatchOpType::Erase: return "Erase";
    }
}

inline PatchOpType ToPatchOpType(const std::string &str) {
    if (str == ToString(PatchOpType::Add)) return PatchOpType::Add;
    if (str == ToString(PatchOpType::Remove)) return PatchOpType::Remove;
    if (str == ToString(PatchOpType::Replace)) return PatchOpType::Replace;
    if (str == ToString(PatchOpType::Set)) return PatchOpType::Set;
    if (str == ToString(PatchOpType::PushBack)) return PatchOpType::PushBack;
    if (str == ToString(PatchOpType::PopBack)) return PatchOpType::PopBack;
    if (str == ToString(PatchOpType::Insert)) return PatchOpType::Insert;
    return PatchOpType::Erase;
}
