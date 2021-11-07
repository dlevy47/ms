#include "logger.hh"

Logger::Message Logger::log(
        Level level,
        const char* file,
        int line,
        const char* function) {
    time_t now_timer;
    ::time(&now_timer);

    struct tm now;
    ::localtime_s(&now, &now_timer);

    Message m;

    m.logger = this;
    wcsftime(
            m.timestamp_str,
            sizeof(m.timestamp_str) / sizeof(*m.timestamp_str),
            L"%Y-%m-%d %T",
            &now);
    m.level = level;
    m.file = file;
    m.line = line;
    m.function = function;

    return std::move(m);
}

