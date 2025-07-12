#ifndef LOG_ENTRY_HPP
#define LOG_ENTRY_HPP

#include <string>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FOOTER,
    HEADER
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string message;
    std::string source_file;
    std::string source_function;
    int source_line;
};

#endif // LOG_ENTRY_HPP
