#ifndef LOG_PARSER_HPP
#define LOG_PARSER_HPP

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include "LogEntry.hpp"

class LogParser {
public:
    using ProgressCallback = std::function<void(const std::string&)>;
    
    // Synchronous parsing (original method)
    std::vector<LogEntry> parse(const std::string& file_path);
    
    // Asynchronous parsing with progress updates
    void parseAsync(const std::string& file_path, 
                   std::vector<LogEntry>& entries,
                   std::mutex& entries_mutex,
                   ProgressCallback progress_callback);
    
    // Check if async parsing is in progress
    bool isParsingInProgress() const;
    
    // Stop async parsing
    void stopParsing();

private:
    std::atomic<bool> parsing_active{false};
    std::atomic<bool> stop_requested{false};
    std::thread parsing_thread;
    
    void parseChunk(const std::vector<std::string>& lines,
                   std::vector<LogEntry>& entries,
                   std::mutex& entries_mutex);
};

#endif // LOG_PARSER_HPP
