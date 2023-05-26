#pragma once

#include "nlohmann/json.hpp"

#include "StoreFwd.h"
#include "StoreJsonFormat.h"

namespace nlohmann {
void to_json(json &, const Store &);
} // namespace nlohmann

// Not using `nlohmann::from_json` pattern to avoid getting a reference to a default-constructed, non-transient `Store` instance.
Store JsonToStore(const nlohmann::json &);

nlohmann::json GetStoreJson(const StoreJsonFormat);
