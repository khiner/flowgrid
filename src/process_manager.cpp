#include "process_manager.h"
#include "audio.h"
#include "context.h"

void ProcessManager::on_action(const Action &) {
    update();
}

void ProcessManager::update() {
    if (audio_running != s.audio.running) {
        if (s.audio.running) audio_thread = std::thread(audio);
        else audio_thread.join();
        audio_running = s.audio.running;
    }
}
