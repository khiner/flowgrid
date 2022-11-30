#pragma once

#include <imgui.h>

#include "../Helper/UI.h"

// Adapted from https://github.com/altschuler/imgui-knobs

enum KnobFlags_ {
    KnobFlags_None = 0,
    KnobFlags_NoTitle = 1 << 0,
    KnobFlags_NoInput = 1 << 1,
    KnobFlags_ValueTooltip = 1 << 2,
    KnobFlags_DragHorizontal = 1 << 3,
};
typedef int KnobFlags;

enum KnobVariant_ {
    KnobVariant_Tick = 1 << 0,
    KnobVariant_Dot = 1 << 1,
    KnobVariant_Wiper = 1 << 2,
    KnobVariant_WiperOnly = 1 << 3,
    KnobVariant_WiperDot = 1 << 4,
    KnobVariant_Stepped = 1 << 5,
    KnobVariant_Space = 1 << 6,
};
typedef int KnobVariant;

namespace Knobs {
struct ColorSet {
    ColorSet(const ImColor &base, const ImColor &hovered, const ImColor &active) : base(base), hovered(hovered), active(active) {}
    ColorSet(const ImColor &color) : ColorSet(color, color, color) {}

    ImColor base, hovered, active;
};

bool
Knob(const char *label, float *p_value, float v_min, float v_max, float speed = 0, const char *format = nullptr,
     HJustify h_justify = HJustify_Middle, KnobVariant variant = KnobVariant_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed = 0, const char *format = nullptr,
             HJustify h_justify = HJustify_Middle, KnobVariant variant = KnobVariant_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);
}
