#pragma once

#include <chrono>
#include <format>

using namespace std::chrono_literals; // Support literals like `1s` or `500ms`

// Time declarations inspired by https://stackoverflow.com/a/14391562/780425
using Clock = std::chrono::system_clock; // Main system clock
using fsec = std::chrono::duration<float>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

inline std::string FormatMillis(auto duration) {
    return std::format("{:.3f}ms", std::chrono::duration<float, std::milli>(duration).count());
}
