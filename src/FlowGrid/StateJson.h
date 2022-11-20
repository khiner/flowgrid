#pragma once

#include "App.h"

#include "nlohmann/json.hpp"

namespace nlohmann {
inline void to_json(json &j, const StatePath &path) { j = path.string(); }
inline void from_json(const json &j, StatePath &path) { path = StatePath(j.get<std::string>()); }

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

JsonType(Preferences, recently_opened_paths)

namespace nlohmann {
inline void to_json(json &j, const Primitive &value) {
    if (std::holds_alternative<U32>(value)) {
        j = format("{:#08X}", std::get<U32>(value));
    } else {
        std::visit([&](auto &&inner_value) {
            j = std::forward<decltype(inner_value)>(inner_value);
        }, value);
    }
}
inline void from_json(const json &j, Primitive &field) {
    if (j.is_boolean()) field = j.get<bool>();
    else if (j.is_number_integer()) field = j.get<int>();
    else if (j.is_number_float()) field = j.get<float>();
    else if (j.is_string()) {
        const auto j_string = j.get<string>();
        if (j_string.starts_with("0X")) field = U32(std::stoul(j_string, nullptr, 0));
        else field = j.get<string>();
    } else throw std::runtime_error(format("Could not parse Primitive JSON value: {}", j.dump()));
}

// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
void to_json(json &j, const StateAction &value);
void from_json(const json &j, StateAction &value);
void to_json(json &j, const ProjectAction &value);
void from_json(const json &j, ProjectAction &value);
} // End `nlohmann` namespace

NLOHMANN_JSON_SERIALIZE_ENUM(PatchOpType, {
    { Add, "add" },
    { Remove, "remove" },
    { Replace, "replace" },
})

JsonType(PatchOp, op, value, old) // lower-case since these are deserialized and passed directly to json-lib. todo not the case anymore
JsonType(Patch, ops, base_path)
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

JsonType(set_history_index, index)
JsonType(open_project, path)
JsonType(open_file_dialog, dialog_json)
JsonType(save_project, path)
JsonType(set_value, path, value)
JsonType(set_values, values)
JsonType(toggle_value, path)
JsonType(apply_patch, patch)
JsonType(set_imgui_color_style, id)
JsonType(set_implot_color_style, id)
JsonType(set_flowgrid_color_style, id)
JsonType(set_flowgrid_diagram_color_style, id)
JsonType(set_flowgrid_diagram_layout_style, id)
JsonType(save_faust_file, path)
JsonType(open_faust_file, path)
JsonType(save_faust_svg_file, path)
} // End `Actions` namespace
