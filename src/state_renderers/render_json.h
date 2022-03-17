#pragma once

#include <nlohmann/json.hpp>
#include "../state.h"

using json = nlohmann::json;

json render_json(const State &);
