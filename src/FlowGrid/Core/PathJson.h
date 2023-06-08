#pragma once

#include "Helper/Path.h"

#include "Json.h"

namespace nlohmann {
inline static void to_json(json &j, const fs::path &path) { j = path.string(); }
inline static void from_json(const json &j, fs::path &path) { path = fs::path(j.get<std::string>()); }
} // namespace nlohmann
