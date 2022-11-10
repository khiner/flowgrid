#include "StateJson.h"

// Construct an action by its variant index (which is also its `ID`) and optional JSON representation (not required for empty actions).
// Adapted for JSON from the default-ctor approach here: https://stackoverflow.com/a/60567091/780425
template<action::ID I = 0>
Action create(action::ID index, const json &j) {
    if constexpr (I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? j.get<std::variant_alternative_t<I, Action>>() : create<I + 1>(index - 1, j);
}

namespace nlohmann {
// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
void to_json(json &j, const Action &value) {
    std::visit([&](auto &&inner_value) {
        j = {value.index(), std::forward<decltype(inner_value)>(inner_value)};
    }, value);
}
void from_json(const json &j, Action &value) {
    auto id = j[0].get<action::ID>();
    value = create(id, j[1]);
}
}
