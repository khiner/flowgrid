#include "Primitive.h"

#include "fmt/format.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace nlohmann {
inline void to_json(json &j, const Primitive &value) {
    if (std::holds_alternative<U32>(value)) {
        j = fmt::format("{:#08X}", std::get<U32>(value));
    } else {
        std::visit(
            [&](auto &&inner_value) {
                j = std::forward<decltype(inner_value)>(inner_value);
            },
            value
        );
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
    } else throw std::runtime_error(fmt::format("Could not parse Primitive JSON value: {}", j.dump()));
}
} // namespace nlohmann

string to_string(const Primitive &primitive) { return json(primitive).dump(); }
