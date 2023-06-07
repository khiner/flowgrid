#pragma once

#include "Field.h"

struct ImColor;
using ImGuiColorEditFlags = int;

namespace Stateful::Field {
struct UInt : TypedBase<U32> {
    UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, U32 value = 0, U32 min = 0, U32 max = 100);
    UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value = 0);

    operator bool() const;
    operator int() const;
    operator ImColor() const;

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
} // namespace Stateful::Field
