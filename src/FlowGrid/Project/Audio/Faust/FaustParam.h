#pragma once

#include "Project/Audio/Sample.h" // Must be included before any Faust includes

#include "FaustParamType.h"
#include "UI/NamesAndValues.h"

#include <string_view>
#include <vector>

struct FaustParamsUIStyle;

struct FaustParam {
    FaustParam(const FaustParamsUIStyle &, const FaustParamType type = Type_None, std::string_view label = "", Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {});

    void Draw(const float suggested_height, bool no_label = false) const;

    inline bool IsGroup() const {
        return Type == Type_None || Type == Type_TGroup || Type == Type_HGroup || Type == Type_VGroup;
    }
    inline bool IsWidthExpandable() const {
        return Type == Type_HGroup || Type == Type_VGroup || Type == Type_TGroup || Type == Type_NumEntry || Type == Type_HSlider || Type == Type_HBargraph;
    }
    inline bool IsHeightExpandable() const {
        return Type == Type_VBargraph || Type == Type_VSlider || Type == Type_CheckButton;
    }
    inline bool IsLabelSameLine() const {
        return Type == Type_NumEntry || Type == Type_HSlider || Type == Type_HBargraph || Type == Type_HRadioButtons || Type == Type_Menu || Type == Type_CheckButton;
    }

    const FaustParamsUIStyle &Style;
    const FaustParamType Type;
    const std::string Id, Label; // `id` will be the same as `label` unless it's the special empty group label of '0x00', in which case `label` will be empty.
    Real *Zone; // Only meaningful for widget params (not groups).
    const Real Min, Max; // Only meaningful for sliders, num-entries, and bar graphs.
    const Real Init, Step; // Only meaningful for sliders and num-entries.
    const char *Tooltip; // Only populated for params (not groups).
    const NamesAndValues names_and_values; // Only nonempty for menus and radio buttons.
    std::vector<FaustParam> Children; // Only populated for containers (groups).

private:
    void DrawGroup(float suggested_height, bool no_label = false) const;
    void DrawParam(float suggested_height, bool no_label = false) const;

    float CalcWidth(bool include_label) const;
    float CalcHeight() const;
    float CalcLabelHeight() const;
};
