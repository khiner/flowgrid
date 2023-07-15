#pragma once

#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "FaustBoxChangeListener.h"
#include "FaustDspChangeListener.h"

class dsp;

struct FaustDSP {
    FaustDSP(std::string_view code);
    ~FaustDSP();

    Box Box;
    dsp *Dsp;

    inline void RegisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.insert(listener);
        listener->OnFaustBoxChanged(Box); // Notify the listener of the current DSP.
    }
    inline void UnregisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.erase(listener);
    }

    inline void RegisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        listener->OnFaustDspChanged(Dsp); // Notify the listener of the current DSP.
    }
    inline void UnregisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    void Update(std::string_view code); // Sets `Box`, `Dsp`, and `ErrorMessage` as a side effect.

    std::string ErrorMessage;

private:
    void Init(std::string_view code); // Returns error message if any.
    void Uninit();

    inline void NotifyBoxChangeListeners() const noexcept {
        for (auto *listener : BoxChangeListeners) listener->OnFaustBoxChanged(Box);
    }

    inline void NotifyDspChangeListeners() const noexcept {
        for (auto *listener : DspChangeListeners) listener->OnFaustDspChanged(Dsp);
    }

    inline static std::unordered_set<FaustBoxChangeListener *> BoxChangeListeners;
    inline static std::unordered_set<FaustDspChangeListener *> DspChangeListeners;
};
