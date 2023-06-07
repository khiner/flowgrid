#include "Actions.h"

namespace nlohmann {
void to_json(json &j, const Action::Stateful &action) {
    action.to_json(j);
}
void from_json(const json &j, Action::Stateful &action) {
    Action::Stateful::from_json(j, action);
}
} // namespace nlohmann
