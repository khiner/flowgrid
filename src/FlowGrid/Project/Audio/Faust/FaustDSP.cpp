#include "FaustDSP.h"

#include "Faust.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/FileDialog/FileDialog.h"

static const std::string FaustDspFileExtension = ".dsp";

FaustDSP::FaustDSP(std::string_view code) {
    Init(code);
}
FaustDSP::~FaustDSP() {
    Uninit();
}

void FaustDSP::Init(std::string_view code) {
    if (Dsp || code.empty()) return Uninit();

    createLibContext();

    const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    std::vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    Box = DSPToBoxes("FlowGrid", string(code), argc, argv.data(), &num_inputs, &num_outputs, ErrorMessage);
    if (!Box) destroyLibContext();

    NotifyBoxChangeListeners();

    if (Box && ErrorMessage.empty()) {
        static llvm_dsp_factory *dsp_factory;
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", Box, argc, argv.data(), "", ErrorMessage, optimize_level);
        if (dsp_factory && ErrorMessage.empty()) {
            Dsp = dsp_factory->createDSPInstance();
            if (!Dsp) ErrorMessage = "Successfully created Faust DSP factory, but could not create the Faust DSP instance.";
        }
    } else if (!Box && ErrorMessage.empty()) {
        ErrorMessage = "`DSPToBoxes` returned no error but did not produce a result.";
    }

    NotifyDspChangeListeners();

    NotifyChangeListeners();
}

void FaustDSP::Uninit() {
    if (Dsp) {
        Dsp = nullptr;
        NotifyDspChangeListeners();
        delete Dsp;
        deleteAllDSPFactories(); // There should only be one factory, but using this instead of `deleteDSPFactory` avoids storing another file-scoped variable.
    }
    if (Box) {
        Box = nullptr;
        NotifyBoxChangeListeners();
    }
    destroyLibContext();
    ErrorMessage = "";
}

void FaustDSP::Update(std::string_view code) {
    if (!Dsp && !code.empty()) return Init(code);
    if (Dsp && code.empty()) return Uninit();

    Uninit();
    return Init(code);
}
