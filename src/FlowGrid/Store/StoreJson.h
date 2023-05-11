#pragma once

#include "nlohmann/json_fwd.hpp"

#include "StoreJsonFormat.h"
#include "StoreTypes.h"

#include "../Action/Action.h"

struct GesturesProject {
    const action::Gestures gestures;
    const Count index;
};

Store JsonToStore(const nlohmann::json &);
GesturesProject JsonToGestures(const nlohmann::json &);

nlohmann::json GetStoreJson(StoreJsonFormat format = StateFormat);
