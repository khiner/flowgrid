#include <thread>
#include "context.h"
#include "audio.h"
#include "draw.h"
#include "blockingconcurrentqueue.h"

using namespace moodycamel; // ConcurrentQueue, BlockingConcurrentQueue

Context context{};
Context &c = context; // Convenient shorthand
const State &state = c.s;
const State &s = c.s; // Convenient shorthand

int main(int, const char *argv[]) {
    const auto faust_libraries_path = std::string(argv[0]) + "/../../lib/faust/libraries";
    std::thread audio_thread(audio, std::cref(faust_libraries_path));

    BlockingConcurrentQueue<Action> q;
    std::thread action_consumer([&]() {
        while (s.action_consumer.running) {
            Action a;
            q.wait_dequeue(a);
            c.on_action(a);
        }
    });

    draw(q, s);
    q.enqueue(set_audio_thread_running{false});
    q.enqueue(set_action_consumer_running{false});

    audio_thread.join();
    action_consumer.join();
    return 0;
}
