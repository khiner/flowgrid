#pragma once

struct Project {
    static void Init();
    static void SaveEmptyProject();
    static void RunQueuedActions(bool force_finalize_gesture = false);
};
