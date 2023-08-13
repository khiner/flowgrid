#pragma once

#include <string>
#include <unordered_set>

#include "Core/Container/TextBuffer.h"
#include "Core/Container/Vector.h"
#include "FaustListener.h"

class dsp;

struct FaustDSPs;

struct FaustDSP : Component, Field::ChangeListener {
    FaustDSP(ComponentArgs &&);
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

    void Init(); // Any resulting error message is set in `ErrorMessage`.
    void Uninit();

    void NotifyChangeListeners() const;
    void NotifyBoxChangeListeners() const;
    void NotifyDspChangeListeners() const;

    FaustDSPs *ParentContainer;
};

struct FaustDSPs : Vector<FaustDSP> {
    using Vector<FaustDSP>::Vector;

    inline void RegisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustChanged(faust_dsp->Id, *faust_dsp);
        }
    }
    inline void UnregisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.erase(listener);
    }

    inline void RegisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustBoxChanged(faust_dsp->Id, faust_dsp->Box);
        }
    }
    inline void UnregisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.erase(listener);
    }

    inline void RegisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustDspChanged(faust_dsp->Id, faust_dsp->Dsp);
        }
    }
    inline void UnregisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    inline void NotifyChangeListeners(const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : ChangeListeners) listener->OnFaustChanged(Id, faust_dsp);
    }
    inline void NotifyBoxChangeListeners(const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : BoxChangeListeners) listener->OnFaustBoxChanged(Id, faust_dsp.Box);
    }
    inline void NotifyDspChangeListeners(const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : DspChangeListeners) listener->OnFaustDspChanged(Id, faust_dsp.Dsp);
    }

private:
    void Render() const override;

    inline static std::unordered_set<FaustChangeListener *> ChangeListeners;
    inline static std::unordered_set<FaustBoxChangeListener *> BoxChangeListeners;
    inline static std::unordered_set<FaustDspChangeListener *> DspChangeListeners;
};
