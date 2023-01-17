#pragma once

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

struct Logger {
    enum Level {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        WARNING = 2,
        ERROR = 3,
        FATAL = 4,
    };

    struct Message {
        Logger* logger;
        wchar_t timestamp_str[32];
        Level level;
        const char* file;
        int line;
        const char* function;

        std::wstringstream message;

        template <typename T>
        Message& operator<<(const T& x) {
            if (level >= logger->threshold)
                message << x;

            return *this;
        }

        Message() = default;

        Message(Message&& rhs):
            logger(std::exchange(rhs.logger, nullptr)),
            level(rhs.level),
            file(rhs.file),
            line(rhs.line),
            function(rhs.function),
            message(std::move(rhs.message)) {
            memcpy(timestamp_str, rhs.timestamp_str, sizeof(timestamp_str));
        }

        ~Message() {
            if (!logger)
                return;

            if (level >= logger->threshold) {
                const char* level_str = "?????";
                switch (level) {
                case Logger::DEBUG:
                    level_str = "DEBUG";
                    break;
                case Logger::INFO:
                    level_str = "INFO ";
                    break;
                    // case Logger::WARN:
                case Logger::WARNING:
                    level_str = "WARN ";
                    break;
                case Logger::ERROR:
                    level_str = "ERROR";
                    break;
                case Logger::FATAL:
                    level_str = "FATAL";
                    break;
                }

                std::wstringstream prefix;
                prefix
                    << timestamp_str << " "
                    << "[" << level_str << "]: "
                    << file << ":" << std::setw(4) << std::left << line << " (" << function << "): ";
                for (size_t i = 0, l = logger->targets.size(); i < l; ++i) {
                    (*logger->targets[i])
                        << prefix.str() << message.str() << "\n";
                }
            }
        }
    };

    std::vector<std::wostream*> targets;

    Level threshold;

    static Logger& Global() {
        static Logger global;
        return global;
    }

    Message log(
        Level level,
        const char* file,
        int line,
        const char* function);
};

#define LOG_TO(logger, level) \
    logger.log(level, \
            __FILE__, \
            __LINE__, \
            __func__)

#define LOG(level) \
    LOG_TO(Logger::Global(), Logger::level)
