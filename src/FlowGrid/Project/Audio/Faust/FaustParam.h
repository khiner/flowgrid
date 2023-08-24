#pragma once

#include "Project/Audio/Sample.h"

#include "Core/Primitive/Float.h"
#include "FaustParamBase.h"
#include "UI/NamesAndValues.h"

struct FaustParam : FaustParamBase, Float {
    FaustParam(ComponentArgs &&, const FaustParamsStyle &style, const FaustParamType type = Type_None, std::string_view label = "", Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {});

    void Render(const float suggested_height, bool no_label = false) const override;

    Real *Zone; // Only meaningful for widget params (not groups).
    const Real Min, Max; // Only meaningful for sliders, num-entries, and bar graphs.
    const Real Init, Step; // Only meaningful for sliders and num-entries.
    const char *Tooltip; // Only populated for params (not groups).
    const NamesAndValues names_and_values; // Only nonempty for menus and radio buttons.

    float CalcWidth(bool include_label) const override;

    void Refresh() override;

private:
    void Render() const override { Render(0); }
};
