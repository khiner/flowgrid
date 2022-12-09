#include "StateJson.h"

// Construct an action by its variant index (which is also its `ID`) and optional JSON representation (not required for empty actions).
// Adapted for JSON from the default-ctor approach here: https://stackoverflow.com/a/60567091/780425
template<ID I = 0>
StateAction CreateStateAction(ID index, const json &j) {
    if constexpr (I >= std::variant_size_v<StateAction>) throw std::runtime_error{"StateAction index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? j.get<std::variant_alternative_t<I, StateAction>>() : CreateStateAction<I + 1>(index - 1, j);
}
template<ID I = 0>
ProjectAction CreateProjectAction(ID index, const json &j) {
    if constexpr (I >= std::variant_size_v<ProjectAction>) throw std::runtime_error{"ProjectAction index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? j.get<std::variant_alternative_t<I, ProjectAction>>() : CreateProjectAction<I + 1>(index - 1, j);
}

namespace nlohmann {
// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
void to_json(json &j, const StateAction &value) {
    std::visit(
        [&](auto &&inner_value) {
            j = {value.index(), std::forward<decltype(inner_value)>(inner_value)};
        },
        value
    );
}
void from_json(const json &j, StateAction &value) {
    auto id = j[0].get<ActionID>();
    value = CreateStateAction(id, j[1]);
}
void to_json(json &j, const ProjectAction &value) {
    std::visit(
        [&](auto &&inner_value) {
            j = {value.index(), std::forward<decltype(inner_value)>(inner_value)};
        },
        value
    );
}
void from_json(const json &j, ProjectAction &value) {
    auto id = j[0].get<ActionID>();
    value = CreateProjectAction(id, j[1]);
}
} // namespace nlohmann
