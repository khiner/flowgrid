#pragma once

#include "../../state.h"

using json = nlohmann::json;

State json2state(const json &);
