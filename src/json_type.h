#pragma once

#include "nlohmann/json.hpp"

using namespace nlohmann;

// An exact copy of `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`, but with a shorter name.
// Note: It's probably a good idea to occasionally check the definition in `nlohmann/json.cpp` for any changes.
#define JSON_TYPE(Type, ...)  \
    inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }
