// Adapted from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_sine.c

#include <thread>

#include "context.h"
#include "audio.h"
#include "draw.h"

Context context{};

int main(int, char **) {
    std::thread audio_thread(audio);
    draw();
    context.state.audio.running = false; // TODO applying actions without tracking (everything else in `Context::dispatch`)
    audio_thread.join();
    return 0;
}
