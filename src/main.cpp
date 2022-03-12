#include <thread>

#include "context.h"
#include "audio.h"
#include "draw.h"

Context context{};

int main(int, char **) {
    std::thread audio_thread(audio);
    draw(context, context.state);
    audio_thread.join();
    return 0;
}
