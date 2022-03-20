#pragma once

#include "thread"
#include "action.h"

struct ProcessManager {
    ProcessManager() { update(); }

    void on_action(const Action &);

    std::thread audio_thread;
    bool audio_running{false};
private:
    void update();
};
