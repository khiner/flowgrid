#pragma once

#include <string_view>
#include <vector>

#include "Project/Audio/Sample.h" // Must be included before any Faust includes
using Real = Sample;

struct FaustParam {
    enum Type {
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

    FaustParam(const Type type = FaustParam::Type_None, std::string_view label = "", Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, std::vector<FaustParam> children = {})
        : type(type), id(label), label(label == "0x00" ? "" : label), zone(zone), min(min), max(max), init(init), step(step), tooltip(tooltip), children(std::move(children)) {}

    const FaustParam::Type type;
    const std::string id, label; // `id` will be the same as `label` unless it's the special empty group label of '0x00', in which case `label` will be empty.
    Real *zone; // Only meaningful for widget params (not containers)
    const Real min, max; // Only meaningful for sliders, num-entries, and bar graphs.
    const Real init, step; // Only meaningful for sliders and num-entries.
    const char *tooltip;
    std::vector<FaustParam> children; // Only populated for containers (groups)
};
