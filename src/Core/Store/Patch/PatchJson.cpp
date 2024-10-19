#include "PatchJson.h"

#include <format>

namespace nlohmann {
void to_json(json &j, const PrimitiveVariant &value) {
    if (std::holds_alternative<u32>(value)) {
        j = std::format("{:#08X}", std::get<u32>(value));
    } else if (std::holds_alternative<float>(value) && std::isnan(std::get<float>(value))) {
        j = "NaN";
    } else {
        std::visit([&](auto &&inner_value) { j = std::forward<decltype(inner_value)>(inner_value); }, value);
    }
}
void from_json(const json &j, PrimitiveVariant &field) {
    if (j.is_boolean()) field = j.get<bool>();
    else if (j.is_number_integer()) field = j.get<int>();
    else if (j.is_number_float()) field = j.get<float>();
    else if (j.is_string()) {
        const auto str = j.get<std::string>();
        if (str == "NaN") field = NAN;
        else if (str.starts_with("0X")) field = u32(std::stoul(str, nullptr, 0));
        else field = str;
    } else throw std::runtime_error(std::format("Could not parse Primitive JSON value: {}", j.dump()));
}

void to_json(json &j, const PatchOp &op) {
    j = json{{"op", ToString(op.Op)}};
    optional_to_json(j, "value", op.Value);
    optional_to_json(j, "old", op.Old);
}
void from_json(const json &j, PatchOp &op) {
    op.Op = ToPatchOpType(j.at("op").get<std::string>());
    if (j.contains("value")) optional_from_json(j, "value", op.Value);
    if (j.contains("old")) optional_from_json(j, "old", op.Old);
}
} // namespace nlohmann
