#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Container/PrimitiveVector.h"
#include "Core/Primitive/UInt.h"
#include "Core/ProducerComponentArgs.h"

struct ImVec4;

struct Colors : PrimitiveVector<u32>, ActionProducer<Action::PrimitiveVector<u32>::Any> {
    using typename ActionProducer<typename Action::PrimitiveVector<u32>::Any>::ProducedActionType;
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    Colors(ArgsT &&, u32 size, std::function<const char *(int)> get_name, const bool allow_auto = false);

    static u32 Float4ToU32(const ImVec4 &value);
    static ImVec4 U32ToFloat4(u32 value);

    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67).
    static constexpr u32 AutoColor = 0X00010101;

    void Set(const std::vector<ImVec4> &) const;
    void Set(const std::vector<std::pair<int, ImVec4>> &) const;

    void RenderValueTree(bool annotate, bool auto_select) const override;

protected:
    void Render() const override;

private:
    std::function<const char *(int)> GetName;
    bool AllowAuto;
};
