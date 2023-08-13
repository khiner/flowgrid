#include "FaustParamsUI.h"

#include "FaustParamsUIs.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"

static std::vector<FaustParamsUI> Uis;

void FaustParamsUIs::Render() const {
    if (Uis.empty()) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    Uis.front().Render();
}

void FaustParamsUIs::OnFaustDspChanged(ID, dsp *dsp) {
    Uis.clear();
    if (dsp) {
        Uis.emplace_back(Style);
        dsp->buildUserInterface(&Uis.front());
    }
}
void FaustParamsUIs::OnFaustDspAdded(ID id, dsp *dsp) {
    OnFaustDspChanged(id, dsp);
}
void FaustParamsUIs::OnFaustDspRemoved(ID id) {
    OnFaustDspChanged(id, nullptr);
}
