#ifndef LOG_PARSER_HPP
#define LOG_PARSER_HPP

#include <string>
#include <vector>
#include "LogEntry.hpp"

class LogParser {
public:
    std::vector<LogEntry> parse(const std::string& file_path);
};

#endif // LOG_PARSER_HPP
