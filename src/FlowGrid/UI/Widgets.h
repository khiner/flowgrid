#pragma once

#include "Styling.h"

struct ImVec2;
struct NamesAndValues;

enum KnobFlags_ {
    KnobFlags_None = 0,
    KnobFlags_NoTitle = 1 << 0,
    KnobFlags_NoInput = 1 << 1,
    KnobFlags_ValueTooltip = 1 << 2,
    KnobFlags_DragHorizontal = 1 << 3,
};
using KnobFlags = int;

enum KnobType_ {
    KnobType_Tick = 1 << 0,
    KnobType_Dot = 1 << 1,
    KnobType_Wiper = 1 << 2,
    KnobType_WiperOnly = 1 << 3,
    KnobType_WiperDot = 1 << 4,
    KnobType_Stepped = 1 << 5,
    KnobType_Space = 1 << 6,
};
using KnobType = int;

enum ValueBarFlags_ {
    // todo flag for value text to follow the value like `ImGui::ProgressBar`
    ValueBarFlags_None = 0,
    ValueBarFlags_Vertical = 1 << 0,
    ValueBarFlags_ReadOnly = 1 << 1,
    ValueBarFlags_NoTitle = 1 << 2,
};
using ValueBarFlags = int;

enum RadioButtonsFlags_ {
    RadioButtonsFlags_None = 0,
    RadioButtonsFlags_Vertical = 1 << 0,
    RadioButtonsFlags_NoTitle = 1 << 1,
};
using RadioButtonsFlags = int;

namespace FlowGrid {
bool Knob(const char *label, float *p_value, float v_min, float v_max, float speed = 0, const char *format = nullptr, HJustify h_justify = HJustify_Middle, KnobType variant = KnobType_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed = 0, const char *format = nullptr, HJustify h_justify = HJustify_Middle, KnobType variant = KnobType_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);

// When `ReadOnly` is set, this is similar to `ImGui::ProgressBar`, but it has a horizontal/vertical switch,
// and the value text doesn't follow the value position (it stays in the middle).
// If `ReadOnly` is not set, this delegates to `SliderFloat`/`VSliderFloat`, but renders the value & label independently.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect, respecting the passed in alignment.
// `size` is the rectangle size.
// Assumes the current cursor position is at the desired top-left of the rectangle.
// Assumes the current item width has been set to the desired rectangle width (not including label width).
bool ValueBar(const char *label, float *value, const float rect_height, const float min_value = 0, const float max_value = 1, const ValueBarFlags flags = ValueBarFlags_None, const HJustify h_justify = HJustify_Middle);

// When `ReadOnly` is set, this is similar to `ImGui::ProgressBar`, but it has a horizontal/vertical switch,
// and the value text doesn't follow the value position (it stays in the middle).
// If `ReadOnly` is not set, this delegates to `SliderFloat`/`VSliderFloat`, but renders the value & label independently.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect, respecting the passed in alignment.
// `size` is the rectangle size.
// Assumes the current cursor position is either the desired top-left of the rectangle (or the beginning of the label for a vertical bar with a title).
// Assumes the current item width has been set to the desired rectangle width (not including label width).
bool RadioButtons(const char *label, float *value, const NamesAndValues &names_and_values, const RadioButtonsFlags flags = RadioButtonsFlags_None, const Justify justify = {HJustify_Middle, VJustify_Middle});
float CalcRadioChoiceWidth(std::string_view choice_name);
} // namespace FlowGrid
