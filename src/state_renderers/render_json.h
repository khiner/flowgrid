#pragma once

#include <nlohmann/json.hpp>
#include "../state.h"

// JSON serializers
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, backend, latency, sample_rate, raw, running) // TODO string fields
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, show)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, demo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Sine, on, frequency, amplitude)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, colors, windows, audio, sine, audio);

using json = nlohmann::json;

json render_json(State &);
