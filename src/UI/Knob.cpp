#include "Knob.h"
#include <numbers>
#include <imgui_internal.h>

const float PI = std::numbers::pi_v<float>;

using namespace ImGui;

namespace Knobs {
namespace detail {
void draw_arc1(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImColor color, int num_segments) {
    const auto &start = center + ImVec2{cosf(start_angle), sinf(start_angle)} * radius;
    const auto &end = center + ImVec2{cosf(end_angle), sinf(end_angle)} * radius;

    // Calculate bezier arc points
    const auto &a = start - center;
    const auto &b = end - center;
    const auto q1 = a.x * a.x + a.y * a.y;
    const auto q2 = q1 + a.x * b.x + a.y * b.y;
    const auto k2 = (4.0f / 3.0f) * (sqrtf((2.0f * q1 * q2)) - q2) / (a.x * b.y - a.y * b.x);
    const auto &arc1 = center + a + ImVec2{-k2 * a.y, k2 * a.x};
    const auto &arc2 = center + b + ImVec2{k2 * b.y, -k2 * b.x};

    GetWindowDrawList()->AddBezierCurve(start, arc1, arc2, end, color, thickness, num_segments);
}

void draw_arc(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImColor color, int num_segments, int bezier_count) {
    // Overlap and angle of ends of Bézier curves needs work, only looks good when not transparent
    const auto overlap = thickness * radius * 0.00001f * PI;
    const auto delta = end_angle - start_angle;
    const auto bez_step = 1.0f / float(bezier_count);

    auto mid_angle = start_angle + overlap;
    for (auto i = 0; i < bezier_count - 1; i++) {
        const auto mid_angle2 = delta * bez_step + mid_angle;
        draw_arc1(center, radius, mid_angle - overlap, mid_angle2 + overlap, thickness, color, num_segments);
        mid_angle = mid_angle2;
    }

    draw_arc1(center, radius, mid_angle - overlap, end_angle, thickness, color, num_segments);
}

ColorSet GetPrimaryColorSet() {
    auto *colors = GetStyle().Colors;
    return {colors[ImGuiCol_ButtonActive], colors[ImGuiCol_ButtonHovered], colors[ImGuiCol_ButtonHovered]};
}
ColorSet GetTrackColorSet() {
    const auto *colors = GetStyle().Colors;
    return {colors[ImGuiCol_FrameBg]};
}
ColorSet GetSecondaryColorSet() {
    const auto *colors = GetStyle().Colors;
    const auto &active = ImVec4(
        colors[ImGuiCol_ButtonActive].x * 0.5f,
        colors[ImGuiCol_ButtonActive].y * 0.5f,
        colors[ImGuiCol_ButtonActive].z * 0.5f,
        colors[ImGuiCol_ButtonActive].w);
    const auto &hovered = ImVec4(
        colors[ImGuiCol_ButtonHovered].x * 0.5f,
        colors[ImGuiCol_ButtonHovered].y * 0.5f,
        colors[ImGuiCol_ButtonHovered].z * 0.5f,
        colors[ImGuiCol_ButtonHovered].w);
    return {active, hovered, hovered};
}

template<typename DataType>
struct knob {
    ImVec2 center;
    bool is_active, is_hovered, value_changed;
    float radius, t, angle_min, angle_max, angle;

    knob(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float speed, float radius, const char *format, KnobFlags flags)
        : radius(radius), t(float(*p_value - v_min) / (v_max - v_min)), angle_min(PI * 0.75f), angle_max(PI * 2.25f), angle(angle_min + (angle_max - angle_min) * t) {
        const ImVec2 &radius_2d = {radius, radius};
        center = GetCursorScreenPos() + radius_2d;;

        // Handle dragging
        ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
        if (!(flags & KnobFlags_DragHorizontal)) drag_flags |= ImGuiSliderFlags_Vertical;
        value_changed = DragBehavior(GetID(label), data_type, p_value, speed, &v_min, &v_max, format, drag_flags);
        InvisibleButton(label, radius_2d * 2);
        is_active = IsItemActive();
        is_hovered = IsItemHovered();
    }

    void draw_dot(float size, float radius_ratio) const {
        const auto &color_set = GetPrimaryColorSet();
        GetWindowDrawList()->AddCircleFilled(
            center + ImVec2{cosf(angle), sinf(angle)} * (radius_ratio * this->radius),
            size * this->radius,
            is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base),
            12);
    }

    void draw_tick(float start, float end, float width, float step_angle) const {
        const auto &color_set = GetPrimaryColorSet();
        const auto tick_start = start * radius;
        const auto tick_end = end * radius;
        const ImVec2 &angle_unit = {cosf(step_angle), sinf(step_angle)};

        GetWindowDrawList()->AddLine(
            center + angle_unit * tick_end,
            center + angle_unit * tick_start,
            is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base),
            width * radius);
    }

    void draw_circle(float size) const {
        const auto &color_set = GetSecondaryColorSet();
        GetWindowDrawList()->AddCircleFilled(center, size * radius, is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base));
    }

    void draw_arc(float radius_ratio, float size, float start_angle, float end_angle, const ColorSet &color_set, int segments, int bezier_count) const {
        const auto track_size = size * this->radius * 0.5f + 0.0001f;
        detail::draw_arc(center, radius_ratio * this->radius, start_angle, end_angle, track_size,
            is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base), segments, bezier_count);
    }
};
} // End namespace `detail`

template<typename DataType>
bool KnobBase(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float _speed, const char *format, KnobVariant variant, KnobFlags flags = KnobFlags_None, int steps = 10) {
    const auto speed = _speed == 0 ? (v_max - v_min) / 250.f : _speed;
    PushID(label);
    const auto width = CalcItemWidth();
    PushItemWidth(width);
    BeginGroup();

    // Draw title
    if (!(flags & KnobFlags_NoTitle)) {
        const auto &title_size = CalcTextSize(label, nullptr, false, width);
        SetCursorPosX(GetCursorPosX() + (width - title_size.x) / 2); // Center title
        Text("%s", label);
    }

    // Draw knob
    detail::knob<DataType> knob(label, data_type, p_value, v_min, v_max, speed, width * 0.5f, format, flags);
    switch (variant) {
        case KnobVariant_Tick: {
            knob.draw_circle(0.85);
            knob.draw_tick(0.5, 0.85, 0.08, knob.angle);
            break;
        }
        case KnobVariant_Dot: {
            knob.draw_circle(0.85);
            knob.draw_dot(0.12, 0.6);
            break;
        }
        case KnobVariant_Wiper: {
            knob.draw_circle(0.7);
            knob.draw_arc(0.8, 0.41, knob.angle_min, knob.angle_max, detail::GetTrackColorSet(), 16, 2);
            if (knob.t > 0.01) knob.draw_arc(0.8, 0.43, knob.angle_min, knob.angle, detail::GetPrimaryColorSet(), 16, 2);
            break;
        }
        case KnobVariant_WiperOnly: {
            knob.draw_arc(0.8, 0.41, knob.angle_min, knob.angle_max, detail::GetTrackColorSet(), 32, 2);
            if (knob.t > 0.01) knob.draw_arc(0.8, 0.43, knob.angle_min, knob.angle, detail::GetPrimaryColorSet(), 16, 2);
            break;
        }
        case KnobVariant_WiperDot: {
            knob.draw_circle(0.6);
            knob.draw_arc(0.85, 0.41, knob.angle_min, knob.angle_max, detail::GetTrackColorSet(), 16, 2);
            knob.draw_dot(0.1, 0.85);
            break;
        }
        case KnobVariant_Stepped: {
            for (auto n = 0; n < steps; n++) {
                const auto a = float(n) / float(steps - 1);
                const auto angle = knob.angle_min + (knob.angle_max - knob.angle_min) * a;
                knob.draw_tick(0.7, 0.9, 0.04, angle);
            }

            knob.draw_circle(0.6);
            knob.draw_dot(0.12, 0.4);
            break;
        }
        case KnobVariant_Space: {
            knob.draw_circle(0.3 - knob.t * 0.1);
            if (knob.t > 0.01) {
                knob.draw_arc(0.4, 0.15, knob.angle_min - 1.0, knob.angle - 1.0, detail::GetPrimaryColorSet(), 16, 2);
                knob.draw_arc(0.6, 0.15, knob.angle_min + 1.0, knob.angle + 1.0, detail::GetPrimaryColorSet(), 16, 2);
                knob.draw_arc(0.8, 0.15, knob.angle_min + 3.0, knob.angle + 3.0, detail::GetPrimaryColorSet(), 16, 2);
            }
            break;
        }
        default: break;
    }

    // Draw tooltip
    if (flags & KnobFlags_ValueTooltip && (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || IsItemActive())) {
        BeginTooltip();
        Text(format, *p_value);
        EndTooltip();
    }

    // Draw input
    if (!(flags & KnobFlags_NoInput)) {
        ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
        if (!(flags & KnobFlags_DragHorizontal)) drag_flags |= ImGuiSliderFlags_Vertical;
        if (DragScalar("###knob_drag", data_type, p_value, speed, &v_min, &v_max, format, drag_flags)) knob.value_changed = true;
    }

    EndGroup();
    PopItemWidth();
    PopID();

    return knob.value_changed;
}

bool Knob(const char *label, float *p_value, float v_min, float v_max, float speed, const char *format, KnobVariant variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_Float, p_value, v_min, v_max, speed, format == nullptr ? "%.3f" : format, variant, flags, steps);
}
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed, const char *format, KnobVariant variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_S32, p_value, v_min, v_max, speed, format == nullptr ? "%i" : format, variant, flags, steps);
}

} // End namespace `Knobs`
