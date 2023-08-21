#pragma once

#include "Project/Audio/Sample.h" // Must be included before any Faust includes
using Real = Sample;

#include "UI/NamesAndValues.h"

#include <string_view>
#include <vector>

struct FaustParamsUIStyle;

enum FaustParamType {
    Type_None = 0,
    // Containers
    Type_HGroup,
    Type_VGroup,
    Type_TGroup,

    // Widgets
    Type_Button,
    Type_CheckButton,
    Type_VSlider,
    Type_HSlider,
    Type_NumEntry,
    Type_HBargraph,
    Type_VBargraph,

    // Types specified with metadata
    Type_Knob,
    Type_Menu,
    Type_VRadioButtons,
    Type_HRadioButtons,
};

struct FaustParam {
    FaustParam(const FaustParamsUIStyle &style, const FaustParamType type = Type_None, std::string_view label = "", Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {}, std::vector<FaustParam> children = {})
        : Style(style), Type(type), Id(label), Label(label == "0x00" ? "" : label), Zone(zone), Min(min), Max(max), Init(init), Step(step), Tooltip(tooltip), names_and_values(std::move(names_and_values)), Children(std::move(children)) {}

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
    Real *Zone; // Only meaningful for widget params (not containers).
    const Real Min, Max; // Only meaningful for sliders, num-entries, and bar graphs.
    const Real Init, Step; // Only meaningful for sliders and num-entries.
    const char *Tooltip;
    const NamesAndValues names_and_values; // Only nonempty for menus and radio buttons.
    std::vector<FaustParam> Children; // Only populated for containers (groups).

private:
    void DrawGroup(const float suggested_height, bool no_label = false) const;
    void DrawParam(const float suggested_height, bool no_label = false) const;

    float CalcWidth(const bool include_label) const;
    float CalcHeight() const;
    float CalcLabelHeight() const;
};
