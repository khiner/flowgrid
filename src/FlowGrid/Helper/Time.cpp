#include "Time.h"

#include "fmt/chrono.h"

std::string FormatTimeSince(const TimePoint &start) { return fmt::format("{}", Clock::now() - start); }
