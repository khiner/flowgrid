#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "faust/dsp/llvm-dsp.h"

namespace fs = std::filesystem;

/*
A minimal example of using the Faust LLVM backend to compile (DSP source->Boxes->DSP instance).
The logic is similar to FlowGrid's usage.
*/
inline static int RunMinimalFaustLlvmExample() {
    static const std::string FaustCode = R"#(import("stdfaust.lib");
    pitchshifter = vgroup("Pitch Shifter", ef.transpose(
       vslider("window (samples)", 1000, 50, 10000, 1),
       vslider("xfade (samples)", 10, 1, 10000, 1),
       vslider("shift (semitones)", 0, -24, +24, 0.1)
     )
    );
    process = _ : pitchshifter)#"; // Missing semicolon to intentionally produce a parse error.

    createLibContext();

    static const std::string libraries_path = fs::relative("../lib/faust/libraries");
    std::vector<const char *> argv = {"-I", libraries_path.c_str()};
    const int argc = argv.size();

    static int num_inputs, num_outputs;
    std::string error_msg;
    Box box = DSPToBoxes("MinimalFaust", FaustCode, argc, argv.data(), &num_inputs, &num_outputs, error_msg);

    dsp *faust_dsp = nullptr;
    llvm_dsp_factory *dsp_factory = nullptr;
    if (box && error_msg.empty()) {
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("MinimalFaust", box, argc, argv.data(), "", error_msg, optimize_level);
        if (dsp_factory) {
            if (error_msg.empty()) {
                faust_dsp = dsp_factory->createDSPInstance();
                if (!faust_dsp) error_msg = "Successfully created Faust DSP factory, but could not create the Faust DSP instance.";
            } else {
                deleteDSPFactory(dsp_factory);
                dsp_factory = nullptr;
            }
        }
    } else if (!box && error_msg.empty()) {
        error_msg = "`DSPToBoxes` returned no error but did not produce a result.";
    }
    if (faust_dsp) {
        delete faust_dsp;
        faust_dsp = nullptr;
    }
    if (dsp_factory) {
        deleteDSPFactory(dsp_factory);
        dsp_factory = nullptr;
    }

    destroyLibContext();

    if (error_msg.empty()) std::cout << "No error." << std::endl;
    else std::cout << error_msg << std::endl;

    return 0;
}
