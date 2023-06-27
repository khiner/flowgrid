#pragma once

#include "FaustAction.h"
#include "FaustBox.h"
#include "FaustGraph.h"
#include "FaustParams.h"

#include "Core/Action/Actionable.h"
#include "Core/Container/MultilineString.h"

class dsp;

struct Faust : Component, Actionable<Action::Faust> {
    Faust(ComponentArgs &&);
    ~Faust();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    struct FaustLog : Component {
        using Component::Component;

        mutable std::string ErrorMessage;

    protected:
        void Render() const override;
    };

    Box Box;
    dsp *Dsp;
    void InitDsp();
    void UninitDsp();

    Prop_(FaustGraph, Graph, "Faust graph");
    Prop_(FaustParams, Params, "Faust params");
    Prop_(FaustLog, Log, "Faust log");

    Prop(MultilineString, Code, R"#(import("stdfaust.lib");
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
