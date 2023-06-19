#pragma once

#include "Core/Field/Field.h"
#include "UIntAction.h"

struct ImColor;
using ImGuiColorEditFlags = int;

struct UInt : TypedField<U32>, Actionable<Action::Primitive::UInt::Any> {
    UInt(ComponentArgs &&, U32 value = 0, U32 min = 0, U32 max = 100);
    UInt(ComponentArgs &&, std::function<const string(U32)> get_name, U32 value = 0);

    operator bool() const { return Value != 0; }
    operator int() const { return Value; };
    operator ImColor() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void Render(const std::vector<U32> &options) const;
    void ColorEdit4(ImGuiColorEditFlags flags = 0, bool allow_auto = false) const;

    const U32 Min, Max;

    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67).
    static constexpr U32 AutoColor = 0X00010101;

private:
    void Render() const override;
    string ValueName(const U32 value) const;

    const std::optional<std::function<const string(U32)>> GetName{};
};
