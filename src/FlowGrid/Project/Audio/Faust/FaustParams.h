#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Flags.h"
#include "Core/Primitive/Float.h"
#include "UI/Styling.h"

#include "FaustParam.h"

using ImGuiTableFlags = int;

// Subset of `ImGuiTableFlags`.
// Unlike the enums above, this one is not a copy of an ImGui enum.
// They can be converted between each other with `TableFlagsToImGui`.
// todo 'Condensed' preset, with NoHostExtendX, NoBordersInBody, NoPadOuterX
enum TableFlags_ {
    TableFlags_None = 0,
    // Features
    TableFlags_Resizable = 1 << 0,
    TableFlags_Reorderable = 1 << 1,
    TableFlags_Hideable = 1 << 2,
    TableFlags_Sortable = 1 << 3,
    TableFlags_ContextMenuInBody = 1 << 4,
    // Borders
    TableFlags_BordersInnerH = 1 << 5,
    TableFlags_BordersOuterH = 1 << 6,
    TableFlags_BordersInnerV = 1 << 7,
    TableFlags_BordersOuterV = 1 << 8,
    TableFlags_Borders = TableFlags_BordersInnerH | TableFlags_BordersOuterH | TableFlags_BordersInnerV | TableFlags_BordersOuterV,
    TableFlags_NoBordersInBody = 1 << 9,
    // Padding
    TableFlags_PadOuterX = 1 << 10,
    TableFlags_NoPadOuterX = 1 << 11,
    TableFlags_NoPadInnerX = 1 << 12,
};

using TableFlags = int;

ImGuiTableFlags TableFlagsToImGui(TableFlags);

inline static const std::vector<Flags::Item> TableFlagItems{
    "Resizable?Enable resizing columns",
    "Reorderable?Enable reordering columns in header row",
    "Hideable?Enable hiding/disabling columns in context menu",
    "Sortable?Enable sorting",
    "ContextMenuInBody?Right-click on columns body/contents will display table context menu. By default it is available in headers row.",
    "BordersInnerH?Draw horizontal borders between rows",
    "BordersOuterH?Draw horizontal borders at the top and bottom",
    "BordersInnerV?Draw vertical borders between columns",
    "BordersOuterV?Draw vertical borders on the left and right sides",
    "NoBordersInBody?Disable vertical borders in columns Body (borders will always appear in Headers)",
    "PadOuterX?Default if 'BordersOuterV' is on. Enable outermost padding. Generally desirable if you have headers.",
    "NoPadOuterX?Default if 'BordersOuterV' is off. Disable outermost padding.",
    "NoPadInnerX?Disable inner padding between columns (double inner padding if 'BordersOuterV' is on, single inner padding if 'BordersOuterV' is off)",
};

enum ParamsWidthSizingPolicy_ {
    ParamsWidthSizingPolicy_StretchToFill, // If a table contains only fixed-width params, allow columns to stretch to fill available width.
    ParamsWidthSizingPolicy_StretchFlexibleOnly, // If a table contains only fixed-width params, it won't stretch to fill available width.
    ParamsWidthSizingPolicy_Balanced, // All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide params).
};
using ParamsWidthSizingPolicy = int;

class dsp;

struct FaustParams : Component {
    using Component::Component;

    void OnDspChanged(dsp *);

    struct Style : Component {
        using Component::Component;

        Prop(Bool, HeaderTitles, true);
        // In frame-height units:
        Prop(Float, MinHorizontalItemWidth, 4, 2, 8);
        Prop(Float, MaxHorizontalItemWidth, 16, 10, 24);
        Prop(Float, MinVerticalItemHeight, 4, 2, 8);
        Prop(Float, MinKnobItemSize, 3, 2, 6);

        Prop(Enum, AlignmentHorizontal, {"Left", "Middle", "Right"}, HJustify_Middle);
        Prop(Enum, AlignmentVertical, {"Top", "Middle", "Bottom"}, VJustify_Middle);
        Prop(Flags, TableFlags, TableFlagItems, TableFlags_Borders | TableFlags_Reorderable | TableFlags_Hideable);
        Prop_(
            Enum, WidthSizingPolicy,
            "?StretchFlexibleOnly: If a table contains only fixed-width paramc, it won't stretch to fill available width.\n"
            "StretchToFill: If a table contains only fixed-width params, allow columns to stretch to fill available width.\n"
            "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide params).",
            {"StretchToFill", "StretchFlexibleOnly", "Balanced"},
            ParamsWidthSizingPolicy_StretchFlexibleOnly
        );

    protected:
        void Render() const override;
    };

    Prop(Style, Style);

protected:
    void Render() const override;

private:
    void DrawUiItem(const FaustParam &, const char *label, const float suggested_height) const;
    float CalcWidth(const FaustParam &, const bool include_label) const;
    float CalcHeight(const FaustParam &) const;
    float CalcLabelHeight(const FaustParam &) const;
};
