#include "LogParser.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

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

void LogParser::parseAsync(const std::string& file_path, 
                          std::vector<LogEntry>& entries,
                          std::mutex& entries_mutex,
                          ProgressCallback progress_callback) {
    // Stop any existing parsing
    stopParsing();
    
    // Start new parsing thread
    parsing_thread = std::thread([this, file_path, &entries, &entries_mutex, progress_callback]() {
        parsing_active = true;
        stop_requested = false;
        
        std::ifstream file(file_path);
        if (!file.is_open()) {
            progress_callback("Error: Could not open file");
            parsing_active = false;
            return;
        }
        
        // Get file size for progress tracking
        auto file_size = std::filesystem::file_size(file_path);
        progress_callback("Starting parse... 0%");
        
        std::string line;
        std::vector<std::string> line_batch;
        const int BATCH_SIZE = 5000;
        int total_lines = 0;
        int total_matched = 0;
        std::streampos current_pos = 0;
        
        while (std::getline(file, line) && !stop_requested) {
            line_batch.push_back(line);
            total_lines++;
            
            // Process batch when we reach BATCH_SIZE
            if (line_batch.size() >= BATCH_SIZE) {
                parseChunk(line_batch, entries, entries_mutex);
                
                // Update progress
                current_pos = file.tellg();
                int progress = (current_pos * 100) / file_size;
                std::string status = "Parsing... " + std::to_string(progress) + "% (" + 
                                   std::to_string(total_lines) + " lines)";
                progress_callback(status);
                
                line_batch.clear();
                
                // Small delay to prevent UI blocking
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        // Process remaining lines
        if (!line_batch.empty() && !stop_requested) {
            parseChunk(line_batch, entries, entries_mutex);
        }
        
        if (stop_requested) {
            progress_callback("Parsing cancelled");
        } else {
            // Get final count
            {
                std::lock_guard<std::mutex> lock(entries_mutex);
                total_matched = entries.size();
            }
            progress_callback("Complete: " + std::to_string(total_matched) + " entries from " + 
                            std::to_string(total_lines) + " lines");
        }
        
        parsing_active = false;
    });
}

void LogParser::parseChunk(const std::vector<std::string>& lines,
                          std::vector<LogEntry>& entries,
                          std::mutex& entries_mutex) {
    std::regex re(R"(\[([^\]]+)\]\[\s*([A-Z]+)\s*\](?:[>:]|\s)*\s*(.*?)\s*\|\s*(.*)\s*->\s*(.*)\(\):\s*(\d+))");
    std::vector<LogEntry> chunk_entries;
    
    for (const auto& line : lines) {
        if (stop_requested) break;
        
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
            chunk_entries.push_back(entry);
        }
    }
    
    // Add chunk to main entries with mutex protection
    {
        std::lock_guard<std::mutex> lock(entries_mutex);
        entries.insert(entries.end(), chunk_entries.begin(), chunk_entries.end());
    }
}

bool LogParser::isParsingInProgress() const {
    return parsing_active;
}

void LogParser::stopParsing() {
    stop_requested = true;
    if (parsing_thread.joinable()) {
        parsing_thread.join();
    }
}
