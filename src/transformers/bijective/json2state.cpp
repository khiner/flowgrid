#include "json2state.h"

State json2state(const json &state_json) {
    return state_json.get<State>();
}
