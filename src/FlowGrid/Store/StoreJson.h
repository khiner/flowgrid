#pragma once

#include "nlohmann/json.hpp"

#include "StoreFwd.h"
#include "StoreJsonFormat.h"

#include "../Action/Action.h"

struct GesturesProject {
    const action::Gestures gestures;
    const Count index;
};

namespace nlohmann {
void to_json(json &, const Store &);
} // namespace nlohmann

Store JsonToStore(const nlohmann::json &);
GesturesProject JsonToGestures(const nlohmann::json &);

nlohmann::json GetStoreJson(StoreJsonFormat format = StateFormat);
