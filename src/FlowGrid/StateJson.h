#pragma once

#include <variant>

#include "App.h"

namespace nlohmann {
inline void to_json(json &j, const JsonPath &path) { j = path.to_string(); }
inline void from_json(const json &j, JsonPath &path) { path = JsonPath(j.get<std::string>()); }

// Convert `std::chrono::time_point`s to/from JSON.
// From https://github.com/nlohmann/json/issues/2159#issuecomment-638104529
template<typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static constexpr inline void to_json(json &j, const std::chrono::time_point<Clock, Duration> &tp) {
        j = tp.time_since_epoch().count();
    }
    static constexpr inline void from_json(const json &j, std::chrono::time_point<Clock, Duration> &tp) {
        Duration duration(j);
        tp = std::chrono::time_point<Clock, Duration>{duration};
    }
};
}

// This boilerplate is for handling `std::optional` values.
// From https://github.com/nlohmann/json/issues/1749#issuecomment-1099890282
template<class J, class T>
constexpr void optional_to_json(J &j, const char *name, const std::optional<T> &value) {
    if (value) j[name] = *value;
}
template<class J, class T>
constexpr void optional_from_json(const J &j, const char *name, std::optional<T> &value) {
    const auto it = j.find(name);
    if (it != j.end()) value = it->template get<T>();
    else value = nullopt;
}

template<typename>
constexpr bool is_optional = false;
template<typename T>
constexpr bool is_optional<std::optional<T>> = true;

template<typename T>
constexpr void extended_to_json(const char *key, json &j, const T &value) {
    if constexpr (is_optional<T>) optional_to_json(j, key, value);
    else j[key] = value;
}
template<typename T>
constexpr void extended_from_json(const char *key, const json &j, T &value) {
    if constexpr (is_optional<T>) optional_from_json(j, key, value);
    else j.at(key).get_to(value);
}

#define ExtendedToJson(v1) extended_to_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
#define ExtendedFromJson(v1) extended_from_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);

#define JsonType(Type, ...) \
    constexpr inline void to_json(nlohmann::json &nlohmann_json_j, const Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedToJson, __VA_ARGS__)) } \
    constexpr inline void from_json(const nlohmann::json &nlohmann_json_j, Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedFromJson, __VA_ARGS__)) }
#define EmptyJsonType(Type) \
    constexpr inline void to_json(nlohmann::json &, const Type &) {} \
    constexpr inline void from_json(const nlohmann::json &, Type &) {}

JsonType(ImVec2, x, y)
JsonType(ImVec2ih, x, y)
JsonType(ImVec4, x, y, z, w)
JsonType(Preferences, recently_opened_paths)

namespace nlohmann {
inline void to_json(json &j, const Primitive &value) {
    std::visit([&](auto &&inner_value) {
        j = std::forward<decltype(inner_value)>(inner_value);
    }, value);
}
inline void from_json(const json &j, Primitive &field) {
    if (j.is_boolean()) field = j.get<bool>();
    else if (j.is_number_integer()) field = j.get<int>();
    else if (j.is_number_float()) field = j.get<float>();
    else if (j.is_string()) field = j.get<string>();
    else if (j.is_object()) field = j;
    else throw std::runtime_error(format("Could not parse Primitive JSON value: {}", j.dump()));
}

// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
inline void to_json(json &j, const Action &value) {
    std::visit([&](auto &&inner_value) {
        j = {value.index(), std::forward<decltype(inner_value)>(inner_value)};
    }, value);
}
inline void from_json(const json &j, Action &value) {
    value = action::create(j[0].get<int>()); // todo fill values
}
}

NLOHMANN_JSON_SERIALIZE_ENUM(JsonPatchOpType, {
    { Add, "add" },
    { Remove, "remove" },
    { Replace, "replace" },
    { Copy, "copy" },
    { Move, "move" },
    { Test, "test" },
})

JsonType(JsonPatchOp, path, op, value, from) // lower-case since these are deserialized and passed directly to json-lib.
JsonType(StatePatch, Patch, Time)

JsonType(FileDialogData, title, filters, file_path, default_file_name, save_mode, max_num_selections, flags)

namespace Actions {
EmptyJsonType(undo)
EmptyJsonType(redo)
EmptyJsonType(open_empty_project)
EmptyJsonType(open_default_project)
EmptyJsonType(show_open_project_dialog)
EmptyJsonType(close_file_dialog)
EmptyJsonType(save_current_project)
EmptyJsonType(save_default_project)
EmptyJsonType(show_save_project_dialog)
EmptyJsonType(close_application)
EmptyJsonType(show_open_faust_file_dialog)
EmptyJsonType(show_save_faust_file_dialog)
EmptyJsonType(show_save_faust_svg_file_dialog)

JsonType(set_history_index, history_index)
JsonType(open_project, path)
JsonType(open_file_dialog, dialog)
JsonType(save_project, path)
JsonType(set_value, path, value)
JsonType(set_values, values)
//JsonType(patch_value, patch)
JsonType(toggle_value, path)
JsonType(set_imgui_settings, settings)
JsonType(set_imgui_color_style, id)
JsonType(set_implot_color_style, id)
JsonType(set_flowgrid_color_style, id)
JsonType(set_flowgrid_diagram_color_style, id)
JsonType(set_flowgrid_diagram_layout_style, id)
JsonType(save_faust_file, path)
JsonType(open_faust_file, path)
JsonType(save_faust_svg_file, path)
} // End `Action` namespace
