#include "Actions.h"

namespace nlohmann {
void to_json(json &j, const Action::Savable &action) {
    action.to_json(j);
}
void from_json(const json &j, Action::Savable &action) {
    Action::Savable::from_json(j, action);
}
} // namespace nlohmann
