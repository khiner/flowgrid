#include "state2json.h"

// JSON serializers
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, running, muted, backend, latency, sample_rate, out_raw) // TODO string fields
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, show)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, demo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UI, running, windows, colors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionConsumer, running)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, ui, audio, action_consumer);

json state2json(const State &s) {
    return s;
}
