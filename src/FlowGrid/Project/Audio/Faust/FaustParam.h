#pragma once

#include <string_view>
#include <vector>

#include "Project/Audio/Sample.h" // Must be included before any Faust includes
using Real = Sample;

#include "UI/NamesAndValues.h"
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
    FaustParam(const FaustParamType type = Type_None, std::string_view label = "", Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {}, std::vector<FaustParam> children = {})
        : Type(type), Id(label), Label(label == "0x00" ? "" : label), Zone(zone), Min(min), Max(max), Init(init), Step(step), Tooltip(tooltip), names_and_values(std::move(names_and_values)), Children(std::move(children)) {}

    const FaustParamType Type;
    const std::string Id, Label; // `id` will be the same as `label` unless it's the special empty group label of '0x00', in which case `label` will be empty.
    Real *Zone; // Only meaningful for widget params (not containers).
    const Real Min, Max; // Only meaningful for sliders, num-entries, and bar graphs.
    const Real Init, Step; // Only meaningful for sliders and num-entries.
    const char *Tooltip;
    const NamesAndValues names_and_values; // Only nonempty for menus and radio buttons.
    std::vector<FaustParam> Children; // Only populated for containers (groups).
};
