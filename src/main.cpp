#include <thread>
#include "faust/dsp/llvm-dsp.h"
//#include "generator/libfaust.h" // For the C++ backend

#include "context.h"
#include "audio.h"
#include "draw.h"

Context context{};

int main(int, char **) {
    std::thread audio_thread(audio);
    draw(context, context.state);
//    createDSPFactoryFromString(m_name_app, theCode, argc, argv, target.c_str(), m_errorString, optimize);
    audio_thread.join();
    return 0;
}
