#pragma once

#include "FaustAction.h"
#include "FaustGraph.h"

// Params

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
    ParamsWidthSizingPolicy_StretchToFill, // If a table contains only fixed-width items, allow columns to stretch to fill available width.
    ParamsWidthSizingPolicy_StretchFlexibleOnly, // If a table contains only fixed-width items, it won't stretch to fill available width.
    ParamsWidthSizingPolicy_Balanced, // All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).
};
using ParamsWidthSizingPolicy = int;

struct Faust : UIStateful {
    using UIStateful::UIStateful;

    void Apply(const Action::FaustAction &) const;
    bool CanApply(const Action::FaustAction &) const;

    bool IsReady() const; // Has code and no errors.
    bool NeedsRestart() const;

    DefineWindow_(
        FaustEditor,
        WindowFlags_MenuBar,

        DefineWindow(Metrics);

        Prop_(Metrics, Metrics, "Faust editor metrics");
    );

    DefineWindow(
        FaustParams,

        DefineUI(
            Style,
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
                "?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
                "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
                "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).",
                {"StretchToFill", "StretchFlexibleOnly", "Balanced"},
                ParamsWidthSizingPolicy_StretchFlexibleOnly
            )
        );

        Prop(Style, Style);
    );

    DefineWindow(
        FaustLog,
        Prop(String, Error);
    );

    Prop_(FaustEditor, Editor, "Faust editor");
    Prop_(FaustGraph, Graph, "Faust graph");
    Prop_(FaustParams, Params, "Faust params");
    Prop_(FaustLog, Log, "Faust log");

    Prop(String, Code, R"#(import("stdfaust.lib");
pitchshifter = vgroup("Pitch Shifter", ef.transpose(
   vslider("window (samples)", 1000, 50, 10000, 1),
   vslider("xfade (samples)", 10, 1, 10000, 1),
   vslider("shift (semitones)", 0, -24, +24, 0.1)
 )
);
process = _ : pitchshifter;)#");
    //    Prop(String, Code, R"#(import("stdfaust.lib");
    // s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
    // process = select2(s,no.noise,os.sawtooth(440));)#");
    //    Prop(String, Code, R"(import("stdfaust.lib");
    // process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)");
    //        Prop(String, Code, R"(import("stdfaust.lib");
    // process = _:fi.highpass(2,1000):_;)");
    //        Prop(String, Code, R"(import("stdfaust.lib");
    // ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
    // q = hslider("q",5,1,30,0.1);
    // gain = hslider("gain",1,0,1,0.01);
    // process = no:noise : fi.resonlp(ctFreq,q,gain);");

    // Based on Faust::UITester.dsp
    //     Prop(String, Code, R"#(import("stdfaust.lib");
    // declare name "UI Tester";
    // declare version "1.0";
    // declare author "O. Guillerminet";
    // declare license "BSD";
    // declare copyright "(c) O. Guillerminet 2012";

    // vbox = vgroup("vbox",
    //     checkbox("check1"),
    //     checkbox("check2"),
    //     nentry("knob0[style:knob]", 60, 0, 127, 0.1)
    // );

    // sliders = hgroup("sliders",
    //     vslider("vslider1", 60, 0, 127, 0.1),
    //     vslider("vslider2", 60, 0, 127, 0.1),
    //     vslider("vslider3", 60, 0, 127, 0.1)
    // );

    // knobs = hgroup("knobs",
    //     vslider("knob1[style:knob]", 60, 0, 127, 0.1),
    //     vslider("knob2[style:knob]", 60, 0, 127, 0.1),
    //     vslider("knob3[style:knob]", 60, 0, 127, 0.1)
    // );

    // smallhbox1 = hgroup("small box 1",
    //     vslider("vslider5 [unit:Hz]", 60, 0, 127, 0.1),
    //     vslider("vslider6 [unit:Hz]", 60, 0, 127, 0.1),
    //     vslider("knob4[style:knob]", 60, 0, 127, 0.1),
    //     nentry("num1 [unit:f]", 60, 0, 127, 0.1),
    //     vbargraph("vbar1", 0, 127)
    // );

    // smallhbox2 = hgroup("small box 2",
    //     vslider("vslider7 [unit:Hz]", 60, 0, 127, 0.1),
    //     vslider("vslider8 [unit:Hz]", 60, 0, 127, 0.1),
    //     vslider("knob5[style:knob]", 60, 0, 127, 0.1),
    //     nentry("num2 [unit:f]", 60, 0, 127, 0.1),
    //     vbargraph("vbar2", 0, 127)
    // );

    // smallhbox3 = hgroup("small box 3",
    //     vslider("vslider9 [unit:Hz]", 60, 0, 127, 0.1),
    //     vslider("vslider10 [unit:m]", 60, 0, 127, 0.1),
    //     vslider("knob6[style:knob]", 60, 0, 127, 0.1),
    //     nentry("num3 [unit:f]", 60, 0, 127, 0.1),
    //     vbargraph("vbar3", 0, 127)
    // );

    // subhbox1 = hgroup("sub box 1",
    //     smallhbox2,
    //     smallhbox3
    // );

    // vmisc = vgroup("vmisc",
    //     vslider("vslider4 [unit:Hz]", 60, 0, 127, 0.1),
    //     button("button"),
    //     hslider("hslider [unit:Hz]", 60, 0, 127, 0.1),
    //     smallhbox1,
    //     subhbox1,
    //     hbargraph("hbar", 0, 127)
    // );

    // hmisc = hgroup("hmisc",
    //     vslider("vslider4 [unit:f]", 60, 0, 127, 0.1),
    //     button("button"),
    //     hslider("hslider", 60, 0, 127, 0.1),
    //     nentry("num [unit:f]", 60, 0, 127, 0.1),
    //     (63.5 : vbargraph("vbar", 0, 127)),
    //     (42.42 : hbargraph("hbar", 0, 127))
    // );

    // //------------------------- Process --------------------------------

    // process = tgroup("grp 1",
    //     vbox,
    //     sliders,
    //     knobs,
    //     vmisc,
    //     hmisc);)#");

protected:
    void Render() const override;
};

extern const Faust &faust;
