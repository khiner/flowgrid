#pragma once

#include "Patch.h"

#include "Core/Json.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace nlohmann {
NLOHMANN_JSON_SERIALIZE_ENUM(
    PatchOpType,
    {
        {PatchOpType::Add, "add"},
        {PatchOpType::Remove, "remove"},
        {PatchOpType::Replace, "replace"},
    }
);
Json(PatchOp, Op, Value, Old);
Json(Patch, Ops, BasePath);
} // namespace nlohmann
