// Adapted from https://github.com/andrewrk/libsoundio/blob/a46b0f21c397cd095319f8c9feccf0f1e50e31ba/example/sio_sine.c

#include <thread>

#include "context.h"
#include "audio.h"
#include "draw.h"

int main(int, char **) {
    Context c{};
    std::thread audio_thread(audio, std::ref(c.state));
    draw(c, c.state);
    c.state.audio.running = false;
    audio_thread.join();
    return 0;
}
