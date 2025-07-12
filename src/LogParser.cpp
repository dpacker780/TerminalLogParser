#include "LogParser.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>

// Simple logging function for LogParser
void logToFile(const std::string& level, const std::string& message) {
    static std::ofstream logFile("logreader_debug.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    
    std::ostringstream oss;
    
    // Windows-compatible time formatting
    std::tm tm_buf;
    std::tm* tm_ptr;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
    tm_ptr = &tm_buf;
#else
    tm_ptr = std::localtime(&time_t);
#endif
    
    oss << "[" << std::put_time(tm_ptr, "%H:%M:%S") 
        << "." << std::setfill('0') << std::setw(3) << ms << "]["
        << level << "]: " << message << " | LogParser.cpp -> parse()" << std::endl;
    
    logFile << oss.str();
    logFile.flush();
}

std::vector<LogEntry> LogParser::parse(const std::string& file_path) {
    std::vector<LogEntry> entries;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        logToFile("ERROR", "Error opening file: " + file_path);
        return entries;
    }
    
    // Get file size for progress tracking
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    logToFile("INFO", "Starting to parse log file: " + file_path + " (" + std::to_string(fileSize) + " bytes)");

    std::string line;
    std::regex re(R"(\[([^\]]+)\]\[\s*([A-Z]+)\s*\](?:[>:]|\s)*\s*(.*?)\s*\|\s*(.*)\s*->\s*(.*)\(\):\s*(\d+))");
    
    int lineCount = 0;
    int matchedLines = 0;
    std::streampos currentPos = 0;

    while (std::getline(file, line)) {
        lineCount++;
        currentPos = file.tellg();
        
        // Log progress every 1000 lines
        if (lineCount % 1000 == 0) {
            int progress = (currentPos * 100) / fileSize;
            logToFile("DEBUG", "Processed " + std::to_string(lineCount) + " lines, " + 
                     std::to_string(matchedLines) + " matches (" + std::to_string(progress) + "%)");
        }
        std::smatch match;
        if (std::regex_match(line, match, re)) {
            LogEntry entry;
            entry.timestamp = match[1];
            std::string level_str = match[2];
            if (level_str == "DEBUG") {
                entry.level = LogLevel::DEBUG;
            } else if (level_str == "INFO") {
                entry.level = LogLevel::INFO;
            } else if (level_str == "WARN") {
                entry.level = LogLevel::WARN;
            } else if (level_str == "ERROR") {
                entry.level = LogLevel::ERROR;
            } else if (level_str == "FOOTER") {
                entry.level = LogLevel::FOOTER;
            } else if (level_str == "HEADER") {
                entry.level = LogLevel::HEADER;
            }
            entry.message = match[3];
            entry.source_file = match[4];
            entry.source_function = match[5];
            entry.source_line = std::stoi(match[6]);
            entries.push_back(entry);
            matchedLines++;
        }
        // Silently skip lines that don't match the regex pattern
    }
    
    logToFile("INFO", "Processed " + std::to_string(lineCount) + " total lines, " + std::to_string(matchedLines) + " matched");
    
    logToFile("INFO", "Finished parsing log file. Found " + std::to_string(entries.size()) + " valid entries");
    return entries;
}
