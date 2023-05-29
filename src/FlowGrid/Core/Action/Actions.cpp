#include "Actions.h"

namespace nlohmann {
void to_json(json &j, const Action::StatefulAction &action) {
    action.to_json(j);
}
void from_json(const json &j, Action::StatefulAction &action) {
    Action::StatefulAction::from_json(j, action);
}
} // namespace nlohmann
