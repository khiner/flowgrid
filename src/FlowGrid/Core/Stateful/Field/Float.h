#pragma once

#include "Field.h"

using ImGuiSliderFlags = int;

namespace Stateful::Field {
struct Float : TypedBase<float> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(
        Stateful::Base *parent, string_view path_segment, string_view name_help,
        float value = 0, float min = 0, float max = 1, const char *fmt = nullptr,
        ImGuiSliderFlags flags = 0, float drag_speed = 0
    );

    void Update() override;

    const float Min, Max, DragSpeed; // If `DragSpeed` is non-zero, this is rendered as an `ImGui::DragFloat`.
    const char *Format;
    const ImGuiSliderFlags Flags;

private:
    void Render() const override;
};
} // namespace Stateful::Field
