#pragma once

// This is in a separate class from `Primitive` because we need the full json header
// rather than just the forward to instantiate definitions for all variant types.
#include "nlohmann/json.hpp"

#include "Primitive.h"

using json = nlohmann::json;

namespace nlohmann {
void to_json(json &, const Primitive &);
void from_json(const json &, Primitive &);
} // namespace nlohmann

std::string to_string(const Primitive &); // Implementation depends on json definitions.
