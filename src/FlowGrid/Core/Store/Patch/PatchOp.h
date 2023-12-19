#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/Primitive.h"

// todo use `IsPrimitive` concept instead of holding `Primitive` values.
// Need to think about how.
struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    Type Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};

std::string to_string(PatchOp::Type);
