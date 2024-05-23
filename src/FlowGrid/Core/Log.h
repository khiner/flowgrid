#pragma once

// Not used yet - placeholder for future use.

#include <format>
#include <map>
#include <source_location>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "Helper/Time.h"

using json = nlohmann::json;
using u32 = unsigned int;

enum class LogLevel {
    Trace = 5,
    Debug = 10,
    Info = 20,
    Warning = 30,
    Error = 40,
    Critical = 50,
};

constexpr std::string LogLevelToString(LogLevel level) {
    using enum LogLevel;
    switch (level) {
        case Trace: return "Trace";
        case Debug: return "Debug";
        case Info: return "Info";
        case Warning: return "Warning";
        case Error: return "Error";
        case Critical: return "Critical";
    }
}

struct LogContext {
    std::string File;
    u32 Line;

    LogContext(const std::string &file, u32 line)
        : File(file), Line(line) {}

    json ToJson() const {
        return {
            {"File", File},
            {"Line", Line},
        };
    }
};

struct MessageMoment {
    std::string Message;
    LogContext Context;
    TimePoint Time;

    MessageMoment(const std::string &message, const LogContext &context, TimePoint time)
        : Message(message), Context(context), Time(time) {}

    json ToJson() const {
        return {
            {"Message", Message},
            {"Context", Context.ToJson()},
            {"Time", std::format("{%Y-%m-%d %T}", Time)},
        };
    }
};

struct Log {
    Log(LogLevel level) : Level(level) {}

    void LogMessage(LogLevel level, const std::string &message, const std::source_location &location = std::source_location::current()) {
        if (level >= Level) {
            MessagesByLevel[level].emplace_back(message, LogContext{location.file_name(), location.line()}, std::chrono::system_clock::now());
        }
    }

    void LogMessage(LogLevel level, std::function<std::string()> callable, const std::source_location &location = std::source_location::current()) {
        if (level >= Level) {
            LogMessage(level, callable(), location);
        }
    }

    json ToJson() const {
        json json;
        for (const auto &[level, messages] : MessagesByLevel) {
            for (const auto &message : messages) {
                json[LogLevelToString(level)].push_back(message.ToJson());
            }
        }
        return json;
    }

    LogLevel Level;

private:
    std::map<LogLevel, std::vector<MessageMoment>> MessagesByLevel;
};

// The following macro assumes that there is a 'Log' instance named 'Logger' in scope.
#define Log(level, message) Logger.LogMessage(level, message)
#define LogIf(level, callable) Logger.LogMessage(level, callable)

/*
int main() {
    Log Logger(LogLevel::Info); // Logger instance is now available in this scope

    Log(LogLevel::Trace, "This is a trace message");
    Log(LogLevel::Debug, "This is a debug message");
    Log(LogLevel::Info, "This is an info message");
    Log(LogLevel::Warning, "This is a warning message");
    Log(LogLevel::Error, "This is an error message");
    Log(LogLevel::Critical, "This is a critical message");

    // Here is an example usage of LogIf:
    // The lambda function only runs when the current log level is Debug or higher
    LogIf(LogLevel::Debug, []() { return "This is an expensive debug message"; });

    std::cout << Logger.ToJson().dump(4);

    return 0;
}
*/
