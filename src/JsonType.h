#pragma once

#include "fmt/chrono.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;
using JsonPath = json::json_pointer;

// Convert `std::chrono::time_point`s to/from JSON.
// From https://github.com/nlohmann/json/issues/2159#issuecomment-638104529
namespace nlohmann {
template<typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static void to_json(json &j, const std::chrono::time_point<Clock, Duration> &tp) {
        j = tp.time_since_epoch().count();
    }

    static void from_json(const json &j, std::chrono::time_point<Clock, Duration> &tp) {
        Duration duration(j);
        tp = std::chrono::time_point<Clock, Duration>{duration};
    }
};
}

// This boilerplate is for handling `std::optional` values.
// From https://github.com/nlohmann/json/issues/1749#issuecomment-1099890282
template<class J, class T>
void optional_to_json(J &j, const char *name, const std::optional<T> &value) {
    if (value) j[name] = *value;
}
template<class J, class T>
void optional_from_json(const J &j, const char *name, std::optional<T> &value) {
    const auto it = j.find(name);
    if (it != j.end()) value = it->template get<T>();
    else value = std::nullopt;
}

template<typename>
constexpr bool is_optional = false;
template<typename T>
constexpr bool is_optional<std::optional<T>> = true;

template<typename T>
void extended_to_json(const char *key, json &j, const T &value) {
    if constexpr (is_optional<T>) optional_to_json(j, key, value);
    else j[key] = value;
}
template<typename T>
void extended_from_json(const char *key, const json &j, T &value) {
    if constexpr (is_optional<T>) optional_from_json(j, key, value);
    else j.at(key).get_to(value);
}

#define ExtendedToJson(v1) extended_to_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
#define ExtendedFromJson(v1) extended_from_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);

#define JsonType(Type, ...) \
    inline void to_json(nlohmann::json &nlohmann_json_j, const Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedToJson, __VA_ARGS__)) } \
    inline void from_json(const nlohmann::json &nlohmann_json_j, Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedFromJson, __VA_ARGS__)) }
