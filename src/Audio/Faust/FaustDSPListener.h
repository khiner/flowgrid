#pragma once

using ID = unsigned int;

struct TransientStore;

class dsp;
struct FaustDSPListener {
    virtual void OnFaustDspChanged(TransientStore &, ID, dsp *) = 0;
    virtual void OnFaustDspAdded(TransientStore &, ID, dsp *) = 0;
    virtual void OnFaustDspRemoved(TransientStore &, ID) = 0;
};
