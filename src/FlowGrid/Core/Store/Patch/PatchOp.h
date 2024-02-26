#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/PrimitiveVariant.h"

enum class PatchOpType {
    Add,
    Remove,
    Replace,
};

struct PatchOp {
    PatchOpType Op{};
    std::optional<PrimitiveVariant> Value{}; // Present for add/replace
    std::optional<PrimitiveVariant> Old{}; // Present for remove/replace
};

inline static std::string to_string(PatchOpType type) {
    switch (type) {
        case PatchOpType::Add: return "Add";
        case PatchOpType::Remove: return "Remove";
        case PatchOpType::Replace: return "Replace";
    }
}
