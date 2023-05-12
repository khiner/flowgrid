#include "PrimitiveJson.h"

#include <format>

namespace nlohmann {
void to_json(json &j, const Primitive &value) {
    if (std::holds_alternative<U32>(value)) {
        j = std::format("{:#08X}", std::get<U32>(value));
    } else if (std::holds_alternative<float>(value) && std::isnan(std::get<float>(value))) {
        j = "NaN";
    } else {
        std::visit(
            [&](auto &&inner_value) {
                j = std::forward<decltype(inner_value)>(inner_value);
            },
            value
        );
    }
}
void from_json(const json &j, Primitive &field) {
    if (j.is_boolean()) field = j.get<bool>();
    else if (j.is_number_integer()) field = j.get<int>();
    else if (j.is_number_float()) field = j.get<float>();
    else if (j.is_string()) {
        const auto j_string = j.get<string>();
        if (j_string == "NaN") field = NAN;
        else if (j_string.starts_with("0X")) field = U32(std::stoul(j_string, nullptr, 0));
        else field = j.get<string>();
    } else throw std::runtime_error(std::format("Could not parse Primitive JSON value: {}", j.dump()));
}
} // namespace nlohmann

string to_string(const Primitive &primitive) { return nlohmann::json(primitive).dump(); }
