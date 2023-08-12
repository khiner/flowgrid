#include "FaustParamsUI.h"

#include "FaustParamsUIs.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"

static std::unique_ptr<FaustParamsUI> Ui;

void FaustParamsUIs::Render() const {
    if (!Ui) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    Ui->Render();
}

void FaustParamsUIs::OnFaustDspChanged(dsp *dsp) {
    if (dsp) {
        Ui = std::make_unique<FaustParamsUI>(Style);
        dsp->buildUserInterface(Ui.get());
    } else {
        Ui = nullptr;
    }
}
