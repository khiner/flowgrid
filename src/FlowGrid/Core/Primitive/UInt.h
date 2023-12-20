#pragma once

#include "Core/Action/Actionable.h"
#include "Primitive.h"
#include "UIntAction.h"

struct ImColor;
using ImGuiColorEditFlags = int;

struct UInt : Primitive<u32>, Actionable<Action::Primitive::UInt::Any> {
    UInt(ComponentArgs &&, u32 value = 0, u32 min = 0, u32 max = 100);
    UInt(ComponentArgs &&, std::function<const string(u32)> get_name, u32 value = 0);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    // `u32` conversion is already provided by `Primitive`.
    operator bool() const { return Value != 0; }
    operator int() const { return Value; };
    operator float() const { return Value; };
    operator ImColor() const;

    void Render(const std::vector<u32> &options) const;

    const u32 Min, Max;

private:
    void Render() const override;
    string ValueName(const u32 value) const;

    const std::optional<std::function<const string(u32)>> GetName{};
};
