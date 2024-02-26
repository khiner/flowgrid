#pragma once

#include "Core/Json.h"
#include "Patch.h"

namespace nlohmann {
void to_json(json &, const PrimitiveVariant &);
void from_json(const json &, PrimitiveVariant &);

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
