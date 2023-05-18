#include "ActionJson.h"

#include "../PrimitiveJson.h"

namespace nlohmann {
// Convert `std::chrono::time_point`s to/from JSON.
// From https://github.com/nlohmann/json/issues/2159#issuecomment-638104529
template<typename Clock, typename Duration>
void adl_serializer<std::chrono::time_point<Clock, Duration>>::to_json(json &j, const std::chrono::time_point<Clock, Duration> &tp) {
    j = tp.time_since_epoch().count();
}
template<typename Clock, typename Duration>
void adl_serializer<std::chrono::time_point<Clock, Duration>>::from_json(const json &j, std::chrono::time_point<Clock, Duration> &tp) {
    Duration duration(j);
    tp = std::chrono::time_point<Clock, Duration>{duration};
}
} // namespace nlohmann

namespace nlohmann {
// This boilerplate is for handling `std::optional` values.
// From https://github.com/nlohmann/json/issues/1749#issuecomment-1099890282
template<class J, class T> constexpr void optional_to_json(J &j, const char *name, const std::optional<T> &value) {
    if (value) j[name] = *value;
}
template<class J, class T> constexpr void optional_from_json(const J &j, const char *name, std::optional<T> &value) {
    const auto it = j.find(name);
    if (it != j.end()) value = it->template get<T>();
    else value = std::nullopt;
}

template<typename> constexpr bool is_optional = false;
template<typename T> constexpr bool is_optional<std::optional<T>> = true;

template<typename T> constexpr void extended_to_json(const char *key, json &j, const T &value) {
    if constexpr (is_optional<T>) optional_to_json(j, key, value);
    else j[key] = value;
}
template<typename T> constexpr void extended_from_json(const char *key, const json &j, T &value) {
    if constexpr (is_optional<T>) optional_from_json(j, key, value);
    else j.at(key).get_to(value);
}

#define ExtendedToJson(v1) extended_to_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
#define ExtendedFromJson(v1) extended_from_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);

#define JsonType(Type, ...)                                                                                                                                                                   \
    void to_json(nlohmann::json &__VA_OPT__(nlohmann_json_j), const Type &__VA_OPT__(nlohmann_json_t)) { __VA_OPT__(NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedToJson, __VA_ARGS__))) } \
    void from_json(const nlohmann::json &__VA_OPT__(nlohmann_json_j), Type &__VA_OPT__(nlohmann_json_t)) { __VA_OPT__(NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedFromJson, __VA_ARGS__))) }
} // namespace nlohmann

namespace nlohmann {
void to_json(json &j, const StorePath &path) { j = path.string(); }
void from_json(const json &j, StorePath &path) { path = StorePath(j.get<std::string>()); }

NLOHMANN_JSON_SERIALIZE_ENUM(
    PatchOp::Type,
    {
        {PatchOp::Type::Add, "add"},
        {PatchOp::Type::Remove, "remove"},
        {PatchOp::Type::Replace, "replace"},
    }
);
JsonType(PatchOp, Op, Value, Old);
JsonType(Patch, Ops, BasePath);
JsonType(StatePatch, Patch, Time);
} // namespace nlohmann

namespace Actions {
JsonType(Undo);
JsonType(Redo);
JsonType(OpenEmptyProject);
JsonType(OpenDefaultProject);
JsonType(ShowOpenProjectDialog);
JsonType(CloseFileDialog);
JsonType(SaveCurrentProject);
JsonType(SaveDefaultProject);
JsonType(ShowSaveProjectDialog);
JsonType(CloseApplication);
JsonType(ShowOpenFaustFileDialog);
JsonType(ShowSaveFaustFileDialog);
JsonType(ShowSaveFaustSvgFileDialog);

JsonType(SetHistoryIndex, index);
JsonType(OpenProject, path);
JsonType(OpenFileDialog, dialog_json);
JsonType(SaveProject, path);
JsonType(SetValue, path, value);
JsonType(SetValues, values);
JsonType(SetVector, path, value);
JsonType(SetMatrix, path, data, row_count);
JsonType(ToggleValue, path);
JsonType(ApplyPatch, patch);
JsonType(SetImGuiColorStyle, id);
JsonType(SetImPlotColorStyle, id);
JsonType(SetFlowGridColorStyle, id);
JsonType(SetGraphColorStyle, id);
JsonType(SetGraphLayoutStyle, id);
JsonType(SaveFaustFile, path);
JsonType(OpenFaustFile, path);
JsonType(SaveFaustSvgFile, path);
} // namespace Actions

namespace nlohmann {
// Construct an action by its variant index (which is also its `ID`) and optional JSON representation (not required for empty actions).
// Adapted for JSON from the default-ctor approach here: https://stackoverflow.com/a/60567091/780425
template<ID I = 0>
action::StatefulAction CreateAction(ID index, const json &j) {
    if constexpr (I >= std::variant_size_v<action::StatefulAction>) throw std::runtime_error{"StatefulAction index " + ::to_string(I + index) + " out of bounds"};
    else return index == 0 ? j.get<std::variant_alternative_t<I, action::StatefulAction>>() : CreateAction<I + 1>(index - 1, j);
}

// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
void to_json(json &j, const action::StatefulAction &value) {
    std::visit(
        [&](auto &&inner_value) {
            j = {value.index(), std::forward<decltype(inner_value)>(inner_value)};
        },
        value
    );
}
void from_json(const json &j, action::StatefulAction &value) {
    auto id = j[0].get<ID>();
    value = CreateAction(id, j[1]);
}
} // namespace nlohmann
