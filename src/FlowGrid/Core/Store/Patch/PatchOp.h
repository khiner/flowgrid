#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/PrimitiveVariant.h"

// todo use `IsPrimitive` concept instead of holding `Primitive` values.
// Need to think about how.

enum PatchOpType {
    Add,
    Remove,
    Replace,
};

struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    PatchOpType Op{};
    std::optional<PrimitiveVariant> Value{}; // Present for add/replace
    std::optional<PrimitiveVariant> Old{}; // Present for remove/replace
};

std::string to_string(PatchOpType);
