#include "Widgets.h"

#include "nlohmann/json.hpp"
#include <format>
#include <numbers>

#include "imgui_internal.h"

#include "../Style.h"

using namespace ImGui;

namespace FlowGrid {
ColorSet GetPrimaryColorSet() { return {GetColorU32(ImGuiCol_ButtonActive), GetColorU32(ImGuiCol_ButtonHovered), GetColorU32(ImGuiCol_ButtonHovered)}; }
ColorSet GetTrackColorSet() { return {GetColorU32(ImGuiCol_FrameBg)}; }

ColorSet GetSecondaryColorSet() {
    // todo make style color
    const auto *colors = GetStyle().Colors;
    const auto &active = ImColor(
        colors[ImGuiCol_ButtonActive].x * 0.5f,
        colors[ImGuiCol_ButtonActive].y * 0.5f,
        colors[ImGuiCol_ButtonActive].z * 0.5f,
        colors[ImGuiCol_ButtonActive].w
    );
    const auto &hovered = ImColor(
        colors[ImGuiCol_ButtonHovered].x * 0.5f,
        colors[ImGuiCol_ButtonHovered].y * 0.5f,
        colors[ImGuiCol_ButtonHovered].z * 0.5f,
        colors[ImGuiCol_ButtonHovered].w
    );
    return {active, hovered, hovered};
}

void HelpMarker(const char *help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help);
        PopTextWrapPos();
        EndTooltip();
    }
}

InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id) {
    auto *window = GetCurrentWindow();
    if (window->SkipItems) return false;

    const auto imgui_id = window->GetID(id);
    const auto size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const auto &cursor = GetCursorScreenPos();
    const ImRect rect{cursor, cursor + size};
    if (!ItemAdd(rect, imgui_id)) return false;

    InteractionFlags flags = InteractionFlags_None;
    static bool hovered, held;
    if (ButtonBehavior(rect, imgui_id, &hovered, &held, ImGuiButtonFlags_AllowItemOverlap)) {
        flags |= InteractionFlags_Clicked;
    }
    if (hovered) flags |= InteractionFlags_Hovered;
    if (held) flags |= InteractionFlags_Held;

    return flags;
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

bool JsonTreeNode(std::string_view label_view, JsonTreeNodeFlags flags, const char *id) {
    const ImU32 highlight_color = style.FlowGrid.Colors[FlowGridCol_HighlightText];
    const auto label = string(label_view);
    const bool highlighted = (flags & JsonTreeNodeFlags_Highlighted);
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;
    const ImGuiTreeNodeFlags imgui_flags = flags & JsonTreeNodeFlags_DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

    if (disabled) BeginDisabled();
    if (highlighted) PushStyleColor(ImGuiCol_Text, highlight_color);
    const bool is_open = id ? TreeNodeEx(id, imgui_flags, "%s", label.c_str()) : TreeNodeEx(label.c_str(), imgui_flags);
    if (highlighted) PopStyleColor();
    if (disabled) EndDisabled();

    return is_open;
}

void JsonTree(std::string_view label_view, const json &value, JsonTreeNodeFlags flags, const char *id) {
    const auto label = string(label_view);
    if (value.is_null()) {
        TextUnformatted(label.empty() ? "(null)" : label.c_str());
    } else if (value.is_object()) {
        if (label.empty() || JsonTreeNode(label, flags, id)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                JsonTree(it.key(), *it, flags);
            }
            if (!label.empty()) TreePop();
        }
    } else if (value.is_array()) {
        if (label.empty() || JsonTreeNode(label, flags, id)) {
            Count i = 0;
            for (const auto &it : value) {
                JsonTree(to_string(i), it, flags);
                i++;
            }
            if (!label.empty()) TreePop();
        }
    } else {
        if (label.empty()) TextUnformatted(value.dump().c_str());
        else Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Knob
// Based on https://github.com/altschuler/imgui-knobs
//-----------------------------------------------------------------------------

const float PI = std::numbers::pi_v<float>;

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
    ImVec2 center;
    bool is_active, is_hovered, value_changed;
    float radius, t, angle_min, angle_max, angle;

    knob(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float speed, float radius, const char *format, KnobFlags flags)
        : radius(radius), t(float(*p_value - v_min) / (v_max - v_min)), angle_min(PI * 0.75f), angle_max(PI * 2.25f), angle(angle_min + (angle_max - angle_min) * t) {
        const ImVec2 &radius_2d = {radius, radius};
        center = GetCursorScreenPos() + radius_2d;

        // Handle dragging
        ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
        if (!(flags & KnobFlags_DragHorizontal)) drag_flags |= ImGuiSliderFlags_Vertical;
        value_changed = DragBehavior(GetID(label), data_type, p_value, speed, &v_min, &v_max, format, drag_flags);
        ImGui::InvisibleButton(label, radius_2d * 2);
        is_active = IsItemActive();
        is_hovered = IsItemHovered();
    }

    void DrawDot(float size, float radius_ratio) const {
        const auto &color_set = GetPrimaryColorSet();
        GetWindowDrawList()->AddCircleFilled(
            center + ImVec2{cosf(angle), sinf(angle)} * (radius_ratio * this->radius),
            size * this->radius,
            is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base),
            12
        );
    }

    void DrawTick(float start, float end, float width, float step_angle) const {
        const auto &color_set = GetPrimaryColorSet();
        const auto tick_start = start * radius;
        const auto tick_end = end * radius;
        const ImVec2 &angle_unit = {cosf(step_angle), sinf(step_angle)};

        GetWindowDrawList()->AddLine(
            center + angle_unit * tick_end,
            center + angle_unit * tick_start,
            is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base),
            width * radius
        );
    }

    void DrawCircle(float size) const {
        const auto &color_set = GetSecondaryColorSet();
        GetWindowDrawList()->AddCircleFilled(center, size * radius, is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base));
    }

    void DrawArc(float radius_ratio, float size, float start_angle, float end_angle, const ColorSet &color_set, int segments, int bezier_count) const {
        const auto track_size = size * this->radius * 0.5f + 0.0001f;
        fg::DrawArc(center, radius_ratio * this->radius, start_angle, end_angle, track_size, is_active ? color_set.active : (is_hovered ? color_set.hovered : color_set.base), segments, bezier_count);
    }
};

template<typename DataType>
bool KnobBase(const char *label, ImGuiDataType data_type, DataType *p_value, DataType v_min, DataType v_max, float _speed, const char *format, HJustify h_justify, KnobVariant variant, KnobFlags flags = KnobFlags_None, int steps = 10) {
    const auto speed = _speed == 0 ? (v_max - v_min) / 250.f : _speed;
    PushID(label);
    const auto width = CalcItemWidth();
    PushItemWidth(width);
    BeginGroup();

    // Draw title
    if (!(flags & KnobFlags_NoTitle)) {
        const float label_w = CalcTextSize(label).x;
        SetCursorPosX(GetCursorPosX() + CalcAlignedX(h_justify, label_w, width, true));
        TextUnformatted(label);
    }

    // Draw knob
    const knob<DataType> knob(label, data_type, p_value, v_min, v_max, speed, width * 0.5f, format, flags);
    switch (variant) {
        case KnobVariant_Tick: {
            knob.DrawCircle(0.85);
            knob.DrawTick(0.5, 0.85, 0.08, knob.angle);
            break;
        }
        case KnobVariant_Dot: {
            knob.DrawCircle(0.85);
            knob.DrawDot(0.12, 0.6);
            break;
        }
        case KnobVariant_Wiper: {
            knob.DrawCircle(0.7);
            knob.DrawArc(0.8, 0.41, knob.angle_min, knob.angle_max, GetTrackColorSet(), 16, 2);
            if (knob.t > 0.01) knob.DrawArc(0.8, 0.43, knob.angle_min, knob.angle, GetPrimaryColorSet(), 16, 2);
            break;
        }
        case KnobVariant_WiperOnly: {
            knob.DrawArc(0.8, 0.41, knob.angle_min, knob.angle_max, GetTrackColorSet(), 32, 2);
            if (knob.t > 0.01) knob.DrawArc(0.8, 0.43, knob.angle_min, knob.angle, GetPrimaryColorSet(), 16, 2);
            break;
        }
        case KnobVariant_WiperDot: {
            knob.DrawCircle(0.6);
            knob.DrawArc(0.85, 0.41, knob.angle_min, knob.angle_max, GetTrackColorSet(), 16, 2);
            knob.DrawDot(0.1, 0.85);
            break;
        }
        case KnobVariant_Stepped: {
            for (auto n = 0; n < steps; n++) {
                const auto a = float(n) / float(steps - 1);
                const auto angle = knob.angle_min + (knob.angle_max - knob.angle_min) * a;
                knob.DrawTick(0.7, 0.9, 0.04, angle);
            }

            knob.DrawCircle(0.6);
            knob.DrawDot(0.12, 0.4);
            break;
        }
        case KnobVariant_Space: {
            knob.DrawCircle(0.3 - knob.t * 0.1);
            if (knob.t > 0.01) {
                knob.DrawArc(0.4, 0.15, knob.angle_min - 1.0, knob.angle - 1.0, GetPrimaryColorSet(), 16, 2);
                knob.DrawArc(0.6, 0.15, knob.angle_min + 1.0, knob.angle + 1.0, GetPrimaryColorSet(), 16, 2);
                knob.DrawArc(0.8, 0.15, knob.angle_min + 3.0, knob.angle + 3.0, GetPrimaryColorSet(), 16, 2);
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

    bool changed = knob.value_changed; // Both the knob and the (optional) input can change the value.

    // Draw input
    if (!(flags & KnobFlags_NoInput)) {
        ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
        if (!(flags & KnobFlags_DragHorizontal)) drag_flags |= ImGuiSliderFlags_Vertical;
        if (DragScalar("###knob_drag", data_type, p_value, speed, &v_min, &v_max, format, drag_flags)) changed = true;
    }

    EndGroup();
    PopItemWidth();
    PopID();

    return changed;
}

bool Knob(const char *label, float *p_value, float v_min, float v_max, float speed, const char *format, HJustify h_justify, KnobVariant variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_Float, p_value, v_min, v_max, speed, format == nullptr ? "%.3f" : format, h_justify, variant, flags, steps);
}
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed, const char *format, HJustify h_justify, KnobVariant variant, KnobFlags flags, int steps) {
    return KnobBase(label, ImGuiDataType_S32, p_value, v_min, v_max, speed, format == nullptr ? "%i" : format, h_justify, variant, flags, steps);
}
}; // namespace FlowGrid
