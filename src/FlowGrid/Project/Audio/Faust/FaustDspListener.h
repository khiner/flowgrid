#pragma once

using ID = unsigned int;

class dsp;
struct FaustDSPListener {
    virtual void OnFaustDspChanged(ID, dsp *) = 0;
    virtual void OnFaustDspAdded(ID, dsp *) = 0;
    virtual void OnFaustDspRemoved(ID) = 0;
};
