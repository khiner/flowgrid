#include <thread>
#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend

#include "context.h"
#include "audio.h"
#include "draw.h"

Context context{};

int main(int argc, const char **argv) {
    std::thread audio_thread(audio);
    draw(context, context.state);
    const std::string faust_code = "import(\"stdfaust.lib\"); process = no.noise;";
    std::string faust_error_msg = "Encountered an error during Faust DSP factory creation";
    auto *faust_dsp_factory = createDSPFactoryFromString("FlowGrid", faust_code, argc, argv, "", faust_error_msg, -1);
    auto *dsp = faust_dsp_factory->createDSPInstance();
    dsp->init(context.state.audio.sample_rate);

    // Faust cleanup
    delete dsp;
    deleteDSPFactory(faust_dsp_factory);
    audio_thread.join();
    return 0;
}
