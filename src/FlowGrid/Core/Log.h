#pragma once

// Not used yet - placeholder for future use.

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Helper/Time.h"

enum class LogLevel {
    Trace = 5,
    Debug = 10,
    Info = 20,
    Warning = 30,
    Error = 40,
    Critical = 50,
};

struct LogContext {
    std::string File;
    int Line;

    LogContext(const std::string &file, int line)
        : File(file), Line(line) {}

    nlohmann::json ToJson() const {
        return {
            {"File", File},
            {"Line", Line},
        };
    }
};

struct MessageMoment {
    std::string Message;
    LogContext ContextInfo;
    TimePoint Time;

    MessageMoment(const std::string &message, const LogContext &contextInfo, TimePoint time)
        : Message(message), ContextInfo(contextInfo), Time(time) {}

    nlohmann::json ToJson() const {
        std::time_t logTime = std::chrono::system_clock::to_time_t(Time);
        return {
            {"Message", Message},
            {"ContextInfo", ContextInfo.ToJson()},
            {"Time", std::ctime(&logTime)},
        };
    }
};

class Log {
    std::map<LogLevel, std::vector<MessageMoment>> MessagesByLevel;
    LogLevel CurrentLevel;

public:
    Log(LogLevel level) : CurrentLevel(level) {}

    LogLevel GetCurrentLogLevel() const {
        return CurrentLevel;
    }

    void LogMessage(LogLevel level, const std::string &message, const LogContext &contextInfo) {
        if (level >= CurrentLevel) {
            MessagesByLevel[level].emplace_back(message, contextInfo, std::chrono::system_clock::now());
        }
    }

    nlohmann::json ToJson() const {
        nlohmann::json json;
        for (const auto &[level, messages] : MessagesByLevel) {
            for (const auto &messageMoment : messages) {
                json[std::to_string(static_cast<int>(level))].push_back(messageMoment.ToJson());
            }
        }
        return json;
    }
};

// The following macro assumes that there is a 'Log' instance named 'Logger' in scope.
#define Log(level, message) Logger.LogMessage(level, message, LogContext(__FILE__, __LINE__))
#define LogIf(level, callable) \
    if (level >= Logger.GetCurrentLogLevel()) { Logger.LogMessage(level, callable(), LogContext(__FILE__, __LINE__)); }

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
