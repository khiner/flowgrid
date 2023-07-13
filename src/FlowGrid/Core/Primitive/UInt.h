#pragma once

#include "PrimitiveField.h"
#include "UIntAction.h"

struct ImColor;
using ImGuiColorEditFlags = int;

struct UInt : PrimitiveField<U32>, Actionable<Action::Primitive::UInt::Any> {
    UInt(ComponentArgs &&, U32 value = 0, U32 min = 0, U32 max = 100);
    UInt(ComponentArgs &&, std::function<const string(U32)> get_name, U32 value = 0);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    operator bool() const { return Value != 0; }
    operator int() const { return Value; };
    operator float() const { return Value; };
    operator ImColor() const;

    void Render(const std::vector<U32> &options) const;

    const U32 Min, Max;

private:
    void Render() const override;
    string ValueName(const U32 value) const;

    const std::optional<std::function<const string(U32)>> GetName{};
};
