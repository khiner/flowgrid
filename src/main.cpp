#include <thread>

#include "context.h"
#include "audio.h"
#include "draw.h"

Context context{};

int main(int, const char *mainArgv[]) {
    const auto faust_libraries_path = std::string(mainArgv[0]) + "/../../lib/faust/libraries";
    std::thread audio_thread(audio, std::cref(faust_libraries_path));
    draw(context, context.state);
    audio_thread.join();
    return 0;
}
