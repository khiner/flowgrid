#pragma once

#include <unordered_set>

#include "Core/Container/TextBuffer.h"
#include "Core/Container/Vector.h"
#include "FaustDSPAction.h"
#include "FaustListener.h"

class dsp;
class llvm_dsp_factory;

struct FaustDSPs;

enum NotificationType {
    Changed,
    Added,
    Removed
};

// `FaustDSP` is a wrapper around a Faust DSP and a Faust Box.
// It owns a Faust DSP code buffer, and updates its DSP and Box instances to reflect the current code.
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

    void Init(bool constructing);
    void Uninit(bool destructing);

    void DestroyDsp();

    void NotifyBoxListeners(NotificationType) const;
    void NotifyDspListeners(NotificationType) const;
    void NotifyListeners(NotificationType) const;

    FaustDSPs *ParentContainer;
    llvm_dsp_factory *DspFactory{nullptr};
};

struct FaustDSPs : Vector<FaustDSP>, Actionable<Action::Faust::DSP::Any> {
    FaustDSPs(ComponentArgs &&);
    ~FaustDSPs();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    inline void RegisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustAdded(faust_dsp->Id, *faust_dsp);
        }
    }
    inline void UnregisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.erase(listener);
    }

    inline void RegisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustBoxAdded(faust_dsp->Id, faust_dsp->Box);
        }
    }
    inline void UnregisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.erase(listener);
    }

    inline void RegisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustDspAdded(faust_dsp->Id, faust_dsp->Dsp);
        }
    }
    inline void UnregisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    inline void NotifyListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : ChangeListeners) {
            if (type == Changed) listener->OnFaustChanged(faust_dsp.Id, faust_dsp);
            else if (type == Added) listener->OnFaustAdded(faust_dsp.Id, faust_dsp);
            else if (type == Removed) listener->OnFaustRemoved(faust_dsp.Id);
        }
    }
    inline void NotifyBoxListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : BoxChangeListeners) {
            if (type == Changed) listener->OnFaustBoxChanged(faust_dsp.Id, faust_dsp.Box);
            else if (type == Added) listener->OnFaustBoxAdded(faust_dsp.Id, faust_dsp.Box);
            else if (type == Removed) listener->OnFaustBoxRemoved(faust_dsp.Id);
        }
    }
    inline void NotifyDspListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept {
        for (auto *listener : DspChangeListeners) {
            if (type == Changed) listener->OnFaustDspChanged(faust_dsp.Id, faust_dsp.Dsp);
            else if (type == Added) listener->OnFaustDspAdded(faust_dsp.Id, faust_dsp.Dsp);
            else if (type == Removed) listener->OnFaustDspRemoved(faust_dsp.Id);
        }
    }

private:
    void Render() const override;

    inline static std::unordered_set<FaustChangeListener *> ChangeListeners;
    inline static std::unordered_set<FaustBoxChangeListener *> BoxChangeListeners;
    inline static std::unordered_set<FaustDspChangeListener *> DspChangeListeners;
};
