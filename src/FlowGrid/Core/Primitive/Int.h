#pragma once

#include "Core/Action/Actionable.h"
#include "IntAction.h"
#include "PrimitiveField.h"

struct Int : PrimitiveField<int>, Actionable<Action::Primitive::Int::Any> {
    Int(ComponentArgs &&, int value = 0, int min = 0, int max = 100);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    operator bool() const { return Value != 0; }
    operator char() const { return Value; };
    operator s8() const { return Value; };
    operator s16() const { return Value; };

    void Render(const std::vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};
