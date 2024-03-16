#include "Widgets.h"

#include <format>
#include <numbers>

#include "imgui_internal.h"

#include "NamesAndValues.h"

using std::string;
using namespace ImGui;

namespace FlowGrid {
inline ImColor ScaleColor(const ImColor &color, const float scale) {
    return ImColor(color.Value.x * scale, color.Value.y * scale, color.Value.z * scale, color.Value.w);
}

bool ValueBar(const char *label, float *value, const float rect_height, const float min_value, const float max_value, const ValueBarFlags flags, const HJustify h_justify) {
    const float rect_width = CalcItemWidth();
    const ImVec2 &size = {rect_width, rect_height};
    const auto &style = GetStyle();
    const bool is_h = !(flags & ValueBarFlags_Vertical);
    const bool has_title = !(flags & ValueBarFlags_NoTitle);
    const auto &draw_list = GetWindowDrawList();

    PushID(label);
    BeginGroup();

    const auto cursor = GetCursorPos();
    if (has_title && !is_h) {
        const float label_w = CalcTextSize(label).x;
        SetCursorPosX(cursor.x + CalcAlignedX(h_justify, label_w, rect_width, true));
        TextUnformatted(label);
    }
    const auto &rect_pos = GetCursorScreenPos();

    bool changed = false;
    if (flags & ValueBarFlags_ReadOnly) {
        const float fraction = (*value - min_value) / max_value;
        RenderFrame(rect_pos, rect_pos + size, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        draw_list->AddRectFilled(
            rect_pos + ImVec2{0, is_h ? 0 : (1 - fraction) * size.y},
            rect_pos + size * ImVec2{is_h ? fraction : 1, 1},
            GetColorU32(ImGuiCol_PlotHistogram),
            style.FrameRounding, is_h ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersBottom
        );
        Dummy(size);
    } else {
        // Draw ImGui widget without value or label text.
        const string id = std::format("##{}", label);
        changed = is_h ? SliderFloat(id.c_str(), value, min_value, max_value, "") : VSliderFloat(id.c_str(), size, value, min_value, max_value, "");
    }

    const string value_text = std::format("{:.2f}", *value);
    const float value_text_w = CalcTextSize(value_text.c_str()).x;
    const float value_text_x = CalcAlignedX(is_h ? HJustify_Middle : h_justify, value_text_w, rect_width);
    draw_list->AddText(rect_pos + ImVec2{value_text_x, (size.y - GetFontSize()) / 2}, GetColorU32(ImGuiCol_Text), value_text.c_str());

    if (has_title && is_h) {
        SameLine();
        TextUnformatted(label);
    }

    EndGroup();
    PopID();

    return !(flags & ValueBarFlags_ReadOnly) && changed; // Read-only value bars never change.
}

float CalcRadioChoiceWidth(const string &choice_name) {
    return CalcTextSize(choice_name).x + GetStyle().ItemInnerSpacing.x + GetFrameHeight();
}

bool RadioButtons(const char *label, float *value, const NamesAndValues &names_and_values, const RadioButtonsFlags flags, const Justify justify) {
    PushID(label);
    BeginGroup();

    const auto &style = GetStyle();
    const bool is_h = !(flags & RadioButtonsFlags_Vertical);
    const float item_width = CalcItemWidth();
    if (!(flags & RadioButtonsFlags_NoTitle)) {
        const float label_width = CalcTextSize(label).x;
        ImVec2 label_pos = GetCursorScreenPos() + (is_h ? ImVec2{0, style.FramePadding.y} : ImVec2{CalcAlignedX(justify.h, label_width, item_width), 0});
        RenderText(label_pos, label);
        Dummy({label_width, GetFrameHeight()});
    }

    bool changed = false;
    for (int i = 0; i < int(names_and_values.names.size()); i++) {
        const string &choice_name = names_and_values.names[i];
        const double choice_value = names_and_values.values[i];
        const float choice_width = CalcRadioChoiceWidth(choice_name);
        if (!is_h) SetCursorPosX(GetCursorPosX() + CalcAlignedX(justify.h, choice_width, item_width));
        else SameLine(0, style.ItemInnerSpacing.x);

        if (RadioButton(choice_name.c_str(), *value == choice_value)) {
            *value = float(choice_value);
            changed = true;
        }
    }
    EndGroup();
    PopID();

    return changed;
}

//-----------------------------------------------------------------------------
// [SECTION] Knob
// Based on https://github.com/altschuler/imgui-knobs
//-----------------------------------------------------------------------------

constexpr float PI = std::numbers::pi_v<float>;

void DrawArc1(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImColor color, int num_segments) {
    const auto &start = center + ImVec2{cosf(start_angle), sinf(start_angle)} * radius;
    const auto &end = center + ImVec2{cosf(end_angle), sinf(end_angle)} * radius;

    // Calculate bezier arc points
    const auto &a = start - center;
    const auto &b = end - center;
    const auto q1 = a.x * a.x + a.y * a.y;
    const auto q2 = q1 + a.x * b.x + a.y * b.y;
    const auto k2 = (4.f / 3.f) * (sqrtf((2.f * q1 * q2)) - q2) / (a.x * b.y - a.y * b.x);
    const auto &arc1 = center + a + ImVec2{-k2 * a.y, k2 * a.x};
    const auto &arc2 = center + b + ImVec2{k2 * b.y, -k2 * b.x};
    GetWindowDrawList()->AddBezierCubic(start, arc1, arc2, end, color, thickness, num_segments);
}

void DrawArc(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImColor color, int num_segments, int bezier_count) {
    // Overlap and angle of ends of Bézier curves needs work, only looks good when not transparent
    const auto overlap = thickness * radius * 0.00001f * PI;
    const auto delta = end_angle - start_angle;
    const auto bez_step = 1.f / float(bezier_count);

    auto mid_angle = start_angle + overlap;
    for (auto i = 0; i < bezier_count - 1; i++) {
        const auto mid_angle2 = delta * bez_step + mid_angle;
        DrawArc1(center, radius, mid_angle - overlap, mid_angle2 + overlap, thickness, color, num_segments);
        mid_angle = mid_angle2;
    }

    DrawArc1(center, radius, mid_angle - overlap, end_angle, thickness, color, num_segments);
}

template<typename DataType>
struct knob {
    knob(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float speed, float radius, const char *format, KnobFlags flags)
        : Radius(radius), ValueRatio(float(*p_value - v_min) / (v_max - v_min)) {
        const ImVec2 &radius_2d = {radius, radius};
        Center = GetCursorScreenPos() + radius_2d;

        // Handle dragging
        ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
        if (!(flags & KnobFlags_DragHorizontal)) drag_flags |= ImGuiSliderFlags_Vertical;
        ValueChanged = DragBehavior(GetID(label), data_type, p_value, speed, &v_min, &v_max, format, drag_flags);
        ImGui::InvisibleButton(label, radius_2d * 2);
        IsActive = IsItemActive();
        IsHovered = IsItemHovered();
    }

    ImU32 GetPrimaryColor() const { return GetColorU32(IsActive ? ImGuiCol_ButtonActive : (IsHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button)); }
    ImU32 GetSecondaryColor() const { return ScaleColor(GetColorU32(IsActive ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered), 0.5); }
    ImU32 GetTrackColor() const { return GetColorU32(ImGuiCol_FrameBg); }

    void Draw(KnobType type, int steps) const {
        switch (type) {
            case KnobType_Tick: {
                DrawCircle(0.85);
                DrawTick(0.5, 0.85, 0.08, Angle);
                return;
            }
            case KnobType_Dot: {
                DrawCircle(0.85);
                DrawDot(0.12, 0.6);
                return;
            }
            case KnobType_Wiper: {
                DrawCircle(0.7);
                DrawArc(0.8, 0.41, AngleMin, AngleMax, GetTrackColor(), 16, 2);
                if (ValueRatio > 0.01) DrawArc(0.8, 0.43, AngleMin, Angle, GetPrimaryColor(), 16, 2);
                return;
            }
            case KnobType_WiperOnly: {
                DrawArc(0.8, 0.41, AngleMin, AngleMax, GetTrackColor(), 32, 2);
                if (ValueRatio > 0.01) DrawArc(0.8, 0.43, AngleMin, Angle, GetPrimaryColor(), 16, 2);
                return;
            }
            case KnobType_WiperDot: {
                DrawCircle(0.6);
                DrawArc(0.85, 0.41, AngleMin, AngleMax, GetTrackColor(), 16, 2);
                DrawDot(0.1, 0.85);
                return;
            }
            case KnobType_Stepped: {
                for (auto n = 0; n < steps; n++) DrawTick(0.7, 0.9, 0.04, AngleMin + (AngleMax - AngleMin) * (float(n) / float(steps - 1)));
                DrawCircle(0.6);
                DrawDot(0.12, 0.4);
                return;
            }
            case KnobType_Space: {
                DrawCircle(0.3 - ValueRatio * 0.1);
                if (ValueRatio > 0.01) {
                    DrawArc(0.4, 0.15, AngleMin - 1.0, Angle - 1.0, GetPrimaryColor(), 16, 2);
                    DrawArc(0.6, 0.15, AngleMin + 1.0, Angle + 1.0, GetPrimaryColor(), 16, 2);
                    DrawArc(0.8, 0.15, AngleMin + 3.0, Angle + 3.0, GetPrimaryColor(), 16, 2);
                }
                break;
            }
        }
    }

    static constexpr float AngleMin{PI * 0.75f}, AngleMax{PI * 2.25f};
    float Radius, ValueRatio;
    float Angle{AngleMin + (AngleMax - AngleMin) * ValueRatio};
    ImVec2 Center;
    bool IsActive, IsHovered, ValueChanged;

private:
    void DrawDot(float size, float radius_ratio) const {
        GetWindowDrawList()->AddCircleFilled(Center + ImVec2{cosf(Angle), sinf(Angle)} * (radius_ratio * Radius), size * Radius, GetPrimaryColor(), 12);
    }

    void DrawCircle(float size) const {
        GetWindowDrawList()->AddCircleFilled(Center, size * Radius, GetSecondaryColor());
    }

    void DrawTick(float start, float end, float width, float step_angle) const {
        const ImVec2 angle_unit{cosf(step_angle), sinf(step_angle)};
        GetWindowDrawList()->AddLine(
            Center + angle_unit * (end * Radius),
            Center + angle_unit * (start * Radius),
            GetPrimaryColor(), width * Radius
        );
    }

    void DrawArc(float radius_ratio, float size, float start_angle, float end_angle, ImU32 color, int segments, int bezier_count) const {
        const auto track_size = size * Radius * 0.5f + 0.0001f;
        FlowGrid::DrawArc(Center, radius_ratio * Radius, start_angle, end_angle, track_size, color, segments, bezier_count);
    }
};

template<typename DataType>
bool KnobBase(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float _speed, const char *format, HJustify h_justify, KnobType type, KnobFlags flags = KnobFlags_None, int steps = 10) {
    const auto speed = _speed == 0 ? (v_max - v_min) / 250.f : _speed;
    const auto width = CalcItemWidth();
    PushID(label);
    PushItemWidth(width);
    BeginGroup();

    // Draw title
    if (!(flags & KnobFlags_NoTitle)) {
        const float label_w = CalcTextSize(label).x;
        SetCursorPosX(GetCursorPosX() + CalcAlignedX(h_justify, label_w, width, true));
        TextUnformatted(label);
    }

    // Draw knob
    const knob<DataType> knob{label, data_type, p_value, v_min, v_max, speed, width * 0.5f, format, flags};
    knob.Draw(type, steps);

    // Draw tooltip
    if (flags & KnobFlags_ValueTooltip && (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || IsItemActive())) {
        BeginTooltip();
        Text(format, *p_value);
        EndTooltip();
    }

    // Draw input. Both the knob and the (optional) input can change the value.
    const bool changed =
        (!(flags & KnobFlags_NoInput) &&
         DragScalar(
             "###knob_drag", data_type, p_value, speed, &v_min, &v_max, format,
             flags & KnobFlags_DragHorizontal ? ImGuiSliderFlags(ImGuiSliderFlags_None) : ImGuiSliderFlags_Vertical
         )) ||
        knob.ValueChanged;

    EndGroup();
    PopItemWidth();
    PopID();

    return changed;
}

bool Knob(const char *label, float *p_value, float v_min, float v_max, float speed, const char *format, HJustify h_justify, KnobType variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_Float, p_value, v_min, v_max, speed, format == nullptr ? "%.3f" : format, h_justify, variant, flags, steps);
}
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed, const char *format, HJustify h_justify, KnobType variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_S32, p_value, v_min, v_max, speed, format == nullptr ? "%i" : format, h_justify, variant, flags, steps);
}
}; // namespace FlowGrid
