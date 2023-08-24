#pragma once

#include "Core/Component.h"
#include "FaustParamBase.h"

struct FaustParamGroup : FaustParamBase, Component {
    FaustParamGroup(ComponentArgs &&args, const FaustParamsStyle &style, const FaustParamType type = Type_None, std::string_view label = "")
        : FaustParamBase(style, type, label), Component(std::move(args)) {}

    void Render(const float suggested_height, bool no_label = false) const override;

private:
    void Render() const override { Render(0); }
};
