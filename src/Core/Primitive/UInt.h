#pragma once

#include "Primitive.h"

struct ImColor;
using ImGuiColorEditFlags = int;

struct UInt : Primitive<u32> {
    UInt(ComponentArgs &&, u32 value = 0, u32 min = 0, u32 max = 100);
    UInt(ComponentArgs &&, std::function<const std::string(u32)> get_name, u32 value = 0);

    // `u32` conversion is already provided by `Primitive`.
    operator bool() const { return Value != 0; }
    operator int() const { return Value; };
    operator size_t() const { return Value; };
    operator float() const { return Value; };
    operator ImColor() const;

    void Render(const std::vector<u32> &options) const;

    const u32 Min, Max;

private:
    void Render() const override;
    std::string ValueName(u32 value) const;

    const std::optional<std::function<const std::string(u32)>> GetName{};
};
