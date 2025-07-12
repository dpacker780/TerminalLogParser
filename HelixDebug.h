#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>

#include <atomic>
#include <iomanip>

///////////////////////////////////////////////////////////////////////
// Handle source_location
// source_location by default gives a very detailed function signature
// in its function_name call. For logging the verbosity can be a bit
// overkill and we can use the more basic version. This is a compile-time
// option to use the more detailed version or not. 
// In the future if we decide we want the more verbose function name we
// can toggle it, and their is a helper function to try to clean the
// output up a bit in cleanFunctionName().

#define _USE_DETAILED_FUNCTION_NAME_IN_SOURCE_LOCATION 0
#include <source_location>


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is my own personal logging class
// v2.6 05/14/2025 -- Read the source code for source_location :), realized there's a format flag, 
//                    _USE_DETAILED_FUNCTION_NAME_IN_SOURCE_LOCATION, and implemented it for 
//                    cleaner function name output, glad to get that annoyance out of the way.
// v2.5 05/13/2025 -- Updated and added some thread-safety needs, updated variable naming to match Helix style
// v2.4 03/17/2025 -- Renamed to the new helix namespace, replaced previous macros, added std::fmt support
//                 -- Made suppressUnit() and unsuppressUnit() thread-safe
//                 -- Made standalone and thread-safe, removed sxTools dependency
// v2.3 07/13/2024 -- Incorporated into the SXTools library [DEPRECATED CHANGE]
// v2.2 07/03/2024 -- Added suppressUnit() and unsuppressUnit() to suppress logging by unit name
// v2.1 07/03/2024 -- Changed format functionality to avoid _CRT_SECURE_NO_WARNINGS
// v2.0 05/29/2024 -- Updated to the C++20 standard, some fixes in the old code
// v1.0 00/00/2010 -- Original implementation

constexpr unsigned int kMessageWidth = 80;
constexpr unsigned int kMaxMessageWidth = 512;

#ifdef ERROR
#undef ERROR
#endif

namespace Helix
{ 
    namespace log 
    {
        // In case we decide to go back to using the more detailed version this
        // flag will determine if formatFunction calls cleanFunctionName or not.

        #if _USE_DETAILED_FUNCTION_NAME_IN_SOURCE_LOCATION == 1
        inline constexpr bool use_detailed_function_name = true;
        #else
        inline constexpr bool use_detailed_function_name = false;
        #endif

        inline constexpr bool is_debug_build = 
            #if defined(__DEBUG__) || defined(_DEBUG) || defined(DEBUG) || defined(__DEBUG) || defined(DEBUG_)
                true;
            #else
                false;
            #endif

        enum class Level 
        { 
            TRACE = 1, 
            DEBUG = 2, 
            INFO = 4, 
            WARN = 8, 
            ERROR = 16,
            HEADER = 32,
            FOOTER = 64,
            NOTICE = 128, // Added NOTICE for informational messages that are not errors or warnings
        };

        // Enable bitwise operators for Level
        constexpr Level operator|(Level lhs, Level rhs) { return static_cast<Level>(static_cast<int>(lhs) | static_cast<int>(rhs)); }
        constexpr Level operator&(Level lhs, Level rhs) { return static_cast<Level>(static_cast<int>(lhs) & static_cast<int>(rhs)); }
        constexpr bool operator==(Level lhs, int rhs) { return static_cast<int>(lhs) == rhs; }
        constexpr bool operator!=(Level lhs, int rhs) { return static_cast<int>(lhs) != rhs; }
        constexpr bool has_level(Level flags, Level level) { return (static_cast<int>(flags) & static_cast<int>(level)) != 0; }

        // Custom format string to ensure we capture std::souce_location
        struct format_string
        {
            std::string_view str;
            std::source_location loc{};

            format_string(std::string_view str, std::source_location loc = std::source_location::current()) : str(str), loc(loc) {}
            format_string(const char* str, std::source_location loc = std::source_location::current()) : str(str), loc(loc) {}
            format_string() = default;
        };

        class hxLogger 
        {

            public:

                static hxLogger& getLogger() 
                {
                    static hxLogger instance;
                    return instance;
                }

                bool isLogLevelEnabled(Level debug_level_flag) const 
                { 
                    std::shared_lock<std::shared_mutex> lock(mSharedMutex);
                    return has_level(mSetLevel, debug_level_flag);
                }

                void turnDebugOn(Level log_flags = Level::TRACE | Level::DEBUG | Level::INFO | Level::WARN | Level::ERROR | Level::HEADER | Level::FOOTER | Level::NOTICE) 
                {
                    if(mIsLogging)
                        return;

                    mLogConsole = mIsLogging = true;
                    mSetLevel = log_flags;
                }

                void changeDebugFlags(Level log_flags = Level::TRACE | Level::DEBUG | Level::INFO | Level::WARN | Level::ERROR | Level::HEADER | Level::FOOTER | Level::NOTICE) 
                {
                    mLogConsole = mIsLogging = true;
                    mSetLevel = log_flags;
                }

                void turnDebugOff() 
                { 
                    mIsLogging = false; 
                }

                void setColorToggle(bool use_color) 
                { 
                    mIsColor = use_color; 
                }

                void useDateTimeToggle(bool use_date) 
                { 
                    mIsDateTime = use_date; 
                }

                void useOStream(std::ostream& out) 
                { 
                    std::lock_guard<std::mutex> lock(mMutex);
                    mOlogStream = &out; 
                }

                void setConsoleLogging(bool log) 
                { 
                    mLogConsole = log; 
                }

                void setFileLogging(bool log) 
                { 
                    mLogFile = log; 
                }

                void setMessageWidth(std::size_t message_width) 
                {  
                    if(!message_width || message_width > kMaxMessageWidth)
                        message_width = kMessageWidth;

                    mMessageWidth = message_width;
                }

                bool configureLogFile(const std::string& filename) 
                {
                    std::lock_guard<std::mutex> lock(mMutex);

                    mFlogStream.open(filename, std::ios::out);
                    mLogFile = mFlogStream.is_open();
                    return mLogFile;
                }

                void log(std::string_view message, Level level, std::string_view line_info = "") 
                {
                    // Take a snapshot of relevant state under mutex protection
                    bool should_log;
                    bool log_to_console;
                    bool log_to_file;
                    bool is_color;
                    bool is_date_time;
                    std::string unit;
                    
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        should_log = mIsLogging && has_level(mSetLevel, level) && (mLogFile || mLogConsole);
                        if (!should_log)
                            return;
                            
                        unit = extractUnitName(line_info);
                        if (m_suppressedUnits.find(unit) != m_suppressedUnits.end())
                            return;
                            
                        log_to_console = mLogConsole;
                        log_to_file = mLogFile && mFlogStream.is_open();
                        is_color = mIsColor;
                        is_date_time = mIsDateTime;
                    }
                    
                    // Special handling for HEADER/FOOTER/NOTICE level - custom width formatting
                    if ((level & Level::HEADER) == Level::HEADER || 
                        (level & Level::FOOTER) == Level::FOOTER ||
                        (level & Level::NOTICE) == Level::NOTICE)
                    {
                        std::string ts_string = timestamp(is_date_time);
                        std::string level_string = warnType(level);
                        std::string level_string_nocolor = warnType(level, true);
                        
                        if (log_to_console) 
                        {
                            // Calculate visible width of the colored message (excluding ANSI codes)
                            std::string clean_message = stripAnsiColors(message);
                            size_t visible_width = clean_message.length();
                            
                            // Calculate padding needed to align with other log messages
                            std::string padded_message = std::string(message);
                            if (visible_width < mMessageWidth) 
                            {
                                size_t padding_needed = mMessageWidth - visible_width;
                                padded_message += std::string(padding_needed, ' ');
                            }
                            
                            std::ostringstream oss;
                            oss << '[' << ts_string << "][" << (is_color ? level_string : level_string_nocolor) << "]: "
                                << padded_message << "| " << line_info;

                            std::lock_guard<std::mutex> lock(mMutex);
                            *mOlogStream << oss.str() << '\n';
                            mOlogStream->flush();
                        }

                        // No colors in file output for headers
                        if (log_to_file) 
                        {
                            // Strip ANSI color codes for file output and apply standard width formatting
                            std::string clean_message = stripAnsiColors(message);
                            std::ostringstream fss;
                            fss << '[' << ts_string << "][" << level_string_nocolor << "]: "
                                << std::left << std::setw(static_cast<int>(mMessageWidth)) << clean_message
                                << "| " << line_info;

                            std::lock_guard<std::mutex> lock(mMutex);
                            mFlogStream << fss.str() << '\n';
                            mFlogStream.flush();
                        }

                        return;
                    }
                    
                    // Process message outside of lock for better performance (normal levels)
                    auto tokens = tokenizeString(message, '\n');
                    tokens = tokenizeLineLength(tokens);

                    std::string ts_string = timestamp(is_date_time);
                    std::string level_string = warnType(level);
                    std::string level_string_nocolor = warnType(level, true);

                    for (std::size_t i = 0; i < tokens.size(); ++i) 
                    {
                        if (log_to_console) 
                        {
                            std::ostringstream oss;
                            oss << '[' << ts_string << "][" << (is_color ? level_string : level_string_nocolor) << ']'
                                << (i == 0 ? ": " : "> ")
                                << std::left << std::setw(static_cast<int>(mMessageWidth)) << tokens[i]
                                << "| " << line_info;

                            std::lock_guard<std::mutex> lock(mMutex);
                            *mOlogStream << oss.str() << '\n';
                            mOlogStream->flush();
                        }

                        // No colors in file output
                        if (log_to_file) 
                        {
                            std::ostringstream fss;
                            fss << '[' << ts_string << "][" << level_string_nocolor << ']'
                                << (i == 0 ? ": " : "> ")
                                << std::left << std::setw(static_cast<int>(mMessageWidth)) << tokens[i]
                                << "| " << line_info;

                            std::lock_guard<std::mutex> lock(mMutex);
                            mFlogStream << fss.str() << '\n';
                            mFlogStream.flush();
                        }
                    }
                }

                // Updated timestamp method to take isDateTime parameter
                std::string timestamp(bool isDateTime) const 
                {
                    auto now = std::chrono::system_clock::now();
                    auto zoned = std::chrono::zoned_time{std::chrono::current_zone(), now};
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
                    
                    if(isDateTime) 
                    {
                        return std::format("{:%Y-%m-%d %H:%M:%OS}.{:03d}", zoned, ms);
                    } 
                    else 
                    {
                        return std::format("{:%H:%M:%OS}.{:03d}", zoned, ms);
                    }
                }

                void suppressUnit(const std::string& unit) 
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    m_suppressedUnits.insert(unit);
                }

                void unsuppressUnit(const std::string& unit) 
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    m_suppressedUnits.erase(unit);
                }

            private:

                hxLogger() 
                {
                    mIsColor = true;
                    mLogFile = false;
                    mIsLogging = false;
                    mLogConsole = false;
                    mIsDateTime = false;
                    mSetLevel = Level::TRACE | Level::DEBUG | Level::INFO | Level::WARN | Level::ERROR | Level::HEADER | Level::FOOTER | Level::NOTICE;
                }

                ~hxLogger() 
                {
                    std::lock_guard<std::mutex> lock(mMutex);

                    if (mFlogStream.is_open())
                        mFlogStream.close();
                }

                std::string warnType(Level level, bool override_color = false) const 
                {
                    bool handle_color = !override_color && mIsColor;

                    if (handle_color) 
                    {
                        switch (level) 
                        {
                            case Level::TRACE:  return "\x1b[37;1m TRACE\x1b[0m";
                            case Level::DEBUG:  return "\x1b[36;1m DEBUG\x1b[0m";
                            case Level::INFO:   return "\x1b[32;1m  INFO\x1b[0m";
                            case Level::WARN:   return "\x1b[33;1m  WARN\x1b[0m";
                            case Level::ERROR:  return "\x1b[31;1m ERROR\x1b[0m";
                            case Level::HEADER: return "\x1b[37;44;1mHEADER\x1b[0m";  // White text on blue background
                            case Level::FOOTER: return "\x1b[37;44;1mFOOTER\x1b[0m";  // White text on blue background  
                            case Level::NOTICE: return "\x1b[35;5;208;1mNOTICE\x1b[0m";  // Orange
                            default:            return "\x1b[36;1m DEBUG\x1b[0m";
                        }
                    } 
                    else 
                    {

                        switch (level) 
                        {
                            case Level::TRACE:  return " TRACE";
                            case Level::DEBUG:  return " DEBUG";
                            case Level::INFO:   return "  INFO";
                            case Level::WARN:   return "  WARN";
                            case Level::ERROR:  return " ERROR";
                            case Level::HEADER: return "HEADER";
                            case Level::FOOTER: return "FOOTER";
                            case Level::NOTICE: return "NOTICE";
                            default:            return " DEBUG";
                        }
                    }
                }

                std::string timestamp() const 
                {
                    auto now = std::chrono::system_clock::now();
                    auto zoned = std::chrono::zoned_time{std::chrono::current_zone(), now};

                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
                    
                    if (mIsDateTime) 
                    {
                        return std::format("{:%Y-%m-%d %H:%M:%OS}.{:03d}", zoned, ms);
                    } 
                    else 
                    {
                        // XXX: std::format %S vs %OS -- %S returns nanoseconds, seems to be a format issue but
                        // %OS shows seconds w/o nanoseconds.
                        return std::format("{:%H:%M:%OS}.{:02d}", zoned, ms);
                    }
                }

                std::vector<std::string> tokenizeString(std::string_view input, char delim) 
                {
                    std::vector<std::string> tokens;
                    std::size_t start = 0;
                    std::size_t end = input.find(delim);

                    while (end != std::string::npos) 
                    {
                        tokens.emplace_back(input.substr(start, end - start));
                        start = end + 1;
                        end = input.find(delim, start);
                    }

                    if(start < input.length())
                        tokens.emplace_back(input.substr(start));

                    return tokens;
                }

                std::vector<std::string> tokenizeLineLength(std::vector<std::string>& strings) 
                {
                    std::vector<std::string> new_tokens;
                    new_tokens.reserve(strings.size());
                    for (auto& str : strings) 
                    {
                        while (str.length() >= mMessageWidth) 
                        {
                            new_tokens.emplace_back(str.substr(0, mMessageWidth));
                            str = str.substr(mMessageWidth);
                        }
                        new_tokens.emplace_back(str);
                    }
                    return new_tokens;
                }

                std::string extractUnitName(std::string_view line_info) const 
                {
                    auto pos = line_info.find_first_of('.');
                    if (pos != std::string_view::npos) {
                        return std::string(line_info.substr(0, pos));
                    }
                    return std::string(line_info);
                }

                // Helper to strip ANSI color codes for file output
                std::string stripAnsiColors(std::string_view text) const 
                {
                    std::string result;
                    result.reserve(text.size());
                    
                    for (size_t i = 0; i < text.size(); ++i) 
                    {
                        if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') 
                        {
                            // Skip ANSI escape sequence
                            i += 2; // Skip '\x1b['
                            while (i < text.size() && text[i] != 'm') 
                            {
                                ++i;
                            }
                            // i now points at 'm' or past end, loop increment will skip it
                        } 
                        else 
                        {
                            result += text[i];
                        }
                    }
                    
                    return result;
                }

                std::unordered_set<std::string> m_suppressedUnits;

                mutable std::mutex mMutex;
                mutable std::shared_mutex mSharedMutex; 
                std::atomic<bool> mIsDateTime{false};
                std::atomic<bool> mIsColor{true};
                std::atomic<bool> mLogFile{false};
                std::atomic<bool> mIsLogging{false};
                std::atomic<bool> mLogConsole{false};
                std::atomic<std::size_t> mMessageWidth{kMessageWidth};
                std::atomic<Level> mSetLevel{Level::TRACE | Level::DEBUG | Level::INFO | Level::WARN | Level::ERROR | Level::HEADER | Level::FOOTER | Level::NOTICE };

                // File I/O
                std::fstream mFlogStream;
                inline static std::ostream* mOlogStream = &std::cout;
        };

        inline bool isLogLevelEnabled(Level debug_level_flag)
        {
            return hxLogger::getLogger().isLogLevelEnabled(debug_level_flag);
        }

        inline bool isDebugLevelEnabled()
        {
            return hxLogger::getLogger().isLogLevelEnabled(Level::DEBUG);
        }

        inline void turnDebugOn(Level log_flags = Level::TRACE | Level::DEBUG | Level::INFO | Level::WARN | Level::ERROR | Level::HEADER | Level::FOOTER | Level::NOTICE) 
        {
            hxLogger::getLogger().turnDebugOn(log_flags);
        }

        inline void turnDebugOff() 
        { 
            hxLogger::getLogger().turnDebugOff(); 
        }

        inline void setMessageWidth(std::size_t message_width) 
        { 
            hxLogger::getLogger().setMessageWidth(message_width); 
        }

        inline void changeDebugFlags(Level log_flags) { hxLogger::getLogger().changeDebugFlags(log_flags); }
        inline void setColorToggle(bool use_color) { hxLogger::getLogger().setColorToggle(use_color); }
        inline void useDateTimeToggle(bool use_date) { hxLogger::getLogger().useDateTimeToggle(use_date); }
        inline void useOStream(std::ostream& out) { hxLogger::getLogger().useOStream(out); }
        inline bool setLogFile(const std::string& filename) { return hxLogger::getLogger().configureLogFile(filename); }
        inline void suppressUnit(const std::string& unit) { hxLogger::getLogger().suppressUnit(unit); }
        inline void unsuppressUnit(const std::string& unit) { hxLogger::getLogger().unsuppressUnit(unit); }

        inline void log(std::string_view message, Level level, std::string_view line_info) 
        {
            hxLogger::getLogger().log(message, level, line_info);
        }

        //////////////////////////////////////////////////////////////////////////////////////
        // Helper functions for logging

        inline std::string cleanFunctionName(std::string_view fullName) 
        {
            // Find the last occurrence of common calling conventions or the last space
            static const std::vector<std::string_view> conventions = {
                "cdecl", 
                "stdcall", 
                "fastcall", 
                "__cdecl", 
                "__stdcall", 
                "__fastcall", 
                "__thiscall"
            };
            
            size_t startPos = 0;
            for (const auto& conv : conventions) 
            {
                size_t pos = fullName.rfind(conv);
                if (pos != std::string_view::npos) 
                {
                    startPos = pos + conv.length();
                    break;
                }
            }
            
            // If no calling convention found, find the last space before the namespace
            if (startPos == 0) 
            {
                size_t lastSpace = fullName.rfind(' ');
                if (lastSpace != std::string_view::npos) 
                {
                    startPos = lastSpace + 1;
                }
            }
            
            // Find the first template bracket or function parameter bracket
            size_t endPos = fullName.find('<', startPos);
            if (endPos == std::string_view::npos) 
            {
                endPos = fullName.find('(', startPos);
                if (endPos == std::string_view::npos) 
                {
                    endPos = fullName.length();
                }
            }
            
            // Extract the clean function name
            std::string cleanName(fullName.substr(startPos, endPos - startPos));
            
            // Trim whitespace
            cleanName.erase(0, cleanName.find_first_not_of(" \t\n\r\f\v"));
            cleanName.erase(cleanName.find_last_not_of(" \t\n\r\f\v") + 1);
            
            return cleanName;
        }

        inline std::string formatLocation(const std::source_location& loc) 
        {
            // If we're using detailed function names from source_location, 
            // try to clean them up so they're not so verbose.

            if(use_detailed_function_name)
            { 
                return std::filesystem::path(loc.file_name()).filename().string() + 
                       " -> " + cleanFunctionName(loc.function_name()) + "(): " + 
                       std::to_string(loc.line());
            }
            else
            {
                return std::filesystem::path(loc.file_name()).filename().string() + 
                       " -> " + loc.function_name() + "(): " + 
                       std::to_string(loc.line());
            }
        }

        template <typename... Args>
        inline void log_fmt(Level level, std::source_location loc, std::string_view fmt_string, Args&&... args)
        {
            try
            { 
                if constexpr(sizeof ...(Args) > 0)
                {
                    log(std::vformat(fmt_string, std::make_format_args(args...)), level, formatLocation(loc));
                } 
                else
                {
                    log(std::vformat(fmt_string, std::make_format_args()), level, formatLocation(loc)); // loc_string);
                }
            }
            catch(const std::exception& e)
            {
                // If formatting fails, log the error message
                log(std::string("Log formatting error: ") + e.what(), Level::ERROR, formatLocation(loc));
            }
        }

        // Special header function that formats message with green '>>>' borders
        template<typename... Args>
        inline void header(const format_string& format, Args&&... args) 
        {
            std::string message;
            if constexpr(sizeof ...(Args) > 0)
            {
                message = std::vformat(format.str, std::make_format_args(args...));
            } 
            else
            {
                message = std::string(format.str);
            }
            
            // Create the formatted header message with green hash borders
            std::string formatted_message;
            if (hxLogger::getLogger().isLogLevelEnabled(Level::HEADER))
            {
                // Calculate padding for centering
                const size_t total_width = 40;  // Total width of the header line
                const size_t hash_count = 8;    // Number of hashes on each side
                size_t text_space = total_width - (hash_count * 2);
                
                if (message.length() > text_space - 2) 
                { // -2 for spaces around text
                    // Text too long, just use minimal formatting
                    formatted_message = "\x1b[32;1m>>>>>>>> " + message + "\x1b[0m";
                } 
                else 
                {
                    // Center the text
                    size_t padding = (text_space - message.length()) / 2;
                    std::string left_hashes(hash_count, '>');
                    std::string left_padding(padding, ' ');
                    std::string right_padding(text_space - message.length() - padding, ' ');
                    
                    formatted_message = "\x1b[32;1m" + left_hashes + left_padding + message + right_padding + "\x1b[0m";
                }
            }
            
            log_fmt(Level::HEADER, format.loc, formatted_message);
        }

        // Special footer function that formats message with blue '<<<' borders
        template<typename... Args>
        inline void footer(const format_string& format, Args&&... args) 
        {
            std::string message;
            if constexpr(sizeof ...(Args) > 0)
            {
                message = std::vformat(format.str, std::make_format_args(args...));
            } 
            else
            {
                message = std::string(format.str);
            }
            
            // Create the formatted footer message with blue '<<<' borders
            std::string formatted_message;
            if (hxLogger::getLogger().isLogLevelEnabled(Level::FOOTER))
            {
                // Calculate padding for centering
                const size_t total_width = 40;  // Total width of the footer line
                const size_t hash_count = 8;    // Number of hashes on each side
                size_t text_space = total_width - (hash_count * 2);
                
                if (message.length() > text_space - 2) 
                { 
                    // -2 for spaces around text
                    // Text too long, just use minimal formatting
                    formatted_message = "\x1b[94;1m<<<<<<<< " + message + "\x1b[0m";
                } 
                else 
                {
                    // Center the text
                    size_t padding = (text_space - message.length()) / 2;
                    std::string left_hashes(hash_count, '<');
                    std::string left_padding(padding, ' ');
                    std::string right_padding(text_space - message.length() - padding, ' ');
                    
                    formatted_message = "\x1b[94;1m" + left_hashes + left_padding + message + right_padding + "\x1b[0m";
                }
            }
            
            log_fmt(Level::FOOTER, format.loc, formatted_message);
        }

        // Special notice function that formats message with orange hash borders
        template<typename... Args>
        inline void notice(const format_string& format, Args&&... args) 
        {
            std::string message;
            if constexpr(sizeof ...(Args) > 0)
            {
                message = std::vformat(format.str, std::make_format_args(args...));
            } 
            else
            {
                message = std::string(format.str);
            }
            
            // Create the formatted footer message
            std::string formatted_message;
            if (hxLogger::getLogger().isLogLevelEnabled(Level::NOTICE))
            {
                // Calculate padding for centering
                const size_t total_width = 40;  // Total width of the footer line
                const size_t hash_count = 8;    // Number of hashes on each side
                size_t text_space = total_width - (hash_count * 2);
                
                if (message.length() > text_space - 2) 
                { 
                    // -2 for spaces around text
                    // Text too long, just use minimal formatting
                    formatted_message = "\x1b[38;5;208;1m######## " + message + "########\x1b[0m";
                } 
                else 
                {
                    // Center the text
                    size_t padding = (text_space - message.length()) / 2;
                    std::string hashes(hash_count, '#');
                    std::string left_padding(padding, ' ');
                    std::string right_padding(text_space - message.length() - padding, ' ');
                    
                    formatted_message = "\x1b[38;5;208;1m" + hashes + left_padding + message + right_padding + hashes + "\x1b[0m";
                }
            }
            
            log_fmt(Level::FOOTER, format.loc, formatted_message);
        }

        template<typename... Args>
        inline void debug(const format_string& format, Args&&... args) 
        {
            if constexpr(is_debug_build)
            { 
                log_fmt(Level::DEBUG, format.loc, format.str, std::forward<Args>(args)...);
            }
        }

        template<typename... Args>
        inline void debugFlagged(bool logMessage, const format_string& format, Args&&... args) 
        {
            if constexpr(is_debug_build)
            { 
                if(logMessage)
                    log_fmt(Level::DEBUG, format.loc, format.str, std::forward<Args>(args)...);
            }
        }

        template<typename... Args>
        inline void trace(const format_string& format, Args&&... args) 
        {
            if constexpr(is_debug_build)
            {
                log_fmt(Level::TRACE, format.loc, format.str, std::forward<Args>(args)...);
            }
        }
        
        template<typename... Args>
        inline void info(const format_string& format, Args&&... args) 
        {
            log_fmt(Level::INFO, format.loc, format.str, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        inline void warn(const format_string& format, Args&&... args) 
        {
            log_fmt(Level::WARN, format.loc, format.str, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        inline void error(const format_string& format, Args&&... args) 
        {
            log_fmt(Level::ERROR, format.loc, format.str, std::forward<Args>(args)...); 
        }

        template<typename Func>
        inline void log_once(Func&& logFunc) 
        {
            if constexpr(is_debug_build)
            {
                static bool logged = false;
                if(!logged)
                {
                    logFunc();
                    logged = true;
                }
            }
        }
       
        inline void log_once_bool(bool condition, const format_string& message_true, const format_string& message_false) 
        {
            log_once([&]() {
                if (condition) 
                {
                    debug(message_true);
                } 
                else 
                {
                    debug(message_false);
                }
            });
        }
        
        inline void log_once_msg(const format_string& message) 
        {
            log_once([&]() 
            {
                debug(message);
            });
        }

    } // namespace log 
} // namespace helix

