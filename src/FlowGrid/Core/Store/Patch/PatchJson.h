#pragma once

#include "Core/Json.h"
#include "Patch.h"

namespace nlohmann {
void to_json(json &, const PrimitiveVariant &);
void from_json(const json &, PrimitiveVariant &);

void to_json(json &, const PatchOp &);
void from_json(const json &, PatchOp &);

Json(Patch, Ops, BasePath);
} // namespace nlohmann
