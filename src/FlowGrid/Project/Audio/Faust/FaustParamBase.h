#pragma once

#include "FaustParamType.h"

#include <string>
#include <string_view>

struct FaustParamsStyle;

struct FaustParamBase {
    FaustParamBase(const FaustParamsStyle &style, const FaustParamType type = Type_None, std::string_view label = "")
        : Style(style), Type(type), ParamId(label), Label(label == "0x00" ? "" : label) {}
    virtual ~FaustParamBase() = default;

    /**
    * `suggested_height == 0` means no height suggestion.
    * For params (as opposed to groups), the suggested height is the expected _available_ height in the group
      (which is relevant for aligning params relative to other params in the same group).
    * Items/groups are allowed to extend beyond this height to fit its contents, if necessary.
    * The cursor position is expected to be set appropriately below the drawn contents.
    */
    virtual void Render(const float suggested_height, bool no_label = false) const = 0;

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

    const FaustParamsStyle &Style;
    const FaustParamType Type;
    const std::string ParamId, Label; // `id` will be the same as `label` unless it's the special empty group label of '0x00', in which case `label` will be empty.

    virtual float CalcWidth(bool include_label) const;
    float CalcHeight() const;
    float CalcLabelHeight() const;
};
