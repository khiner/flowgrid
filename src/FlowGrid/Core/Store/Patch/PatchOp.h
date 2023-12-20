#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/PrimitiveVariant.h"

// todo use `IsPrimitive` concept instead of holding `Primitive` values.
// Need to think about how.
struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    Type Op{};
    std::optional<PrimitiveVariant> Value{}; // Present for add/replace
    std::optional<PrimitiveVariant> Old{}; // Present for remove/replace
};

std::string to_string(PatchOp::Type);
