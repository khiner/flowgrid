#pragma once

#include "Patch.h"

#include "Core/Json.h"
#include "Core/PrimitiveJson.h"

namespace nlohmann {
inline static void to_json(json &j, const fs::path &path) { j = path.string(); }
inline static void from_json(const json &j, fs::path &path) { path = fs::path(j.get<std::string>()); }
} // namespace nlohmann

namespace nlohmann {
NLOHMANN_JSON_SERIALIZE_ENUM(
    PatchOp::Type,
    {
        {PatchOp::Type::Add, "add"},
        {PatchOp::Type::Remove, "remove"},
        {PatchOp::Type::Replace, "replace"},
    }
);
Json(PatchOp, Op, Value, Old);
Json(Patch, Ops, BasePath);
} // namespace nlohmann