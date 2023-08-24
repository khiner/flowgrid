#pragma once

#include "Core/Container/TextBuffer.h"
#include "FaustListener.h"

class dsp;
class llvm_dsp_factory;

struct FaustDSPs;

// `FaustDSP` is a wrapper around a Faust DSP and a Faust Box.
// It owns a Faust DSP code buffer, and updates its DSP and Box instances to reflect the current code.
struct FaustDSP : Component, Field::ChangeListener {
    FaustDSP(ComponentArgs &&, const FaustDSPContainer &);
    ~FaustDSP();

    void OnFieldChanged() override;

    void Update(); // Sets `Box`, `Dsp`, and `ErrorMessage` based on the current `Code`.

    Prop_(TextBuffer, Code, "Faust code", R"#(import("stdfaust.lib");
pitchshifter = vgroup("Pitch Shifter", ef.transpose(
   vslider("window (samples)", 1000, 50, 10000, 1),
   vslider("xfade (samples)", 10, 1, 10000, 1),
   vslider("shift (semitones)", 0, -24, +24, 0.1)
 )
);
process = _ : pitchshifter;)#");

    Box Box;
    dsp *Dsp;
    std::string ErrorMessage;

private:
    void Render() const override;

    void Init(bool constructing);
    void Uninit(bool destructing);

    void DestroyDsp();

    void NotifyBoxListeners(NotificationType) const;
    void NotifyDspListeners(NotificationType) const;
    void NotifyListeners(NotificationType) const;

    const FaustDSPContainer &Container;
    llvm_dsp_factory *DspFactory{nullptr};
};
