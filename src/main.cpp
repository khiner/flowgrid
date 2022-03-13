#include <thread>
#include "faust/dsp/llvm-dsp.h"
#include "generator/libfaust.h" // For the C++ backend

#include "context.h"
#include "audio.h"
#include "draw.h"
#include <filesystem>

namespace fs = std::filesystem;

Context context{};

int main(int, const char *mainArgv[]) {
    std::thread audio_thread(audio);
    draw(context, context.state);
    auto faust_libraries_path = fs::path(mainArgv[0]).append("../../lib/faust/libraries").string();

    int argc = 0;
    const char **argv = new const char *[128];
    argv[argc++] = "-I";
    argv[argc++] = &faust_libraries_path[0]; // convert to string
//    argv[argc++] = "-vec";
//    argv[argc++] = "-vs";
//    argv[argc++] = "128";
//    argv[argc++] = "-dfs";
    const int optimize = -1;

    const std::string faust_code = "import(\"stdfaust.lib\"); process = no.noise;";
    std::string faust_error_msg = "Encountered an error during Faust DSP factory creation";
    auto *faust_dsp_factory = createDSPFactoryFromString("FlowGrid", faust_code, argc, argv, "", faust_error_msg, optimize);
    auto *dsp = faust_dsp_factory->createDSPInstance();
    dsp->init(context.state.audio.sample_rate);

    // Faust cleanup
    delete dsp;
    deleteDSPFactory(faust_dsp_factory);

    audio_thread.join();
    return 0;
}
