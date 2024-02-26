#pragma once

#include <optional>
#include <string>

#include "Core/Primitive/PrimitiveVariant.h"

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
