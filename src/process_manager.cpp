#include "process_manager.h"
#include "audio.h"
#include "context.h"

void ProcessManager::on_action(const Action &) {
    update();
}

void ProcessManager::update() {
    if (!audio_running && s.audio.running) {
        audio_thread = std::thread(audio);
        audio_running = true;
    } else if (audio_running && !s.audio.running) {
        audio_thread.join();
        audio_running = false;
    }
}
