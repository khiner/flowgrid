#pragma once

#include "Core/Action/Actionable.h"
#include "FloatAction.h"
#include "Primitive.h"

using ImGuiSliderFlags = int;

struct Float : Primitive<float>, Actionable<Action::Primitive::Float::Any> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(ComponentArgs &&, float value = 0, float min = 0, float max = 1, const char *fmt = nullptr, ImGuiSliderFlags flags = 0, float drag_speed = 0);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    const float Min, Max, DragSpeed; // If `DragSpeed` is non-zero, this is rendered as an `ImGui::DragFloat`.
    const char *Format;
    const ImGuiSliderFlags Flags;

private:
    void Render() const override;
};
