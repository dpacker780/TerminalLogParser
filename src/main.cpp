#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "LogParser.hpp"
#include "LogEntry.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#undef ERROR
#undef RGB
#undef DOUBLE
#else
#include <cstdlib>
#endif

using namespace ftxui;

// =============================================================================
// Platform-specific initialization
// =============================================================================

#ifdef _WIN32
void enableWindowsConsoleColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
#endif

// =============================================================================
// Configuration management
// =============================================================================

std::string getConfigPath() {
    return "logreader_config.txt";
}

std::string loadLastFilePath() {
    std::ifstream config(getConfigPath());
    if (config.is_open()) {
        std::string path;
        std::getline(config, path);
        config.close();
        if (!path.empty() && std::filesystem::exists(path)) {
            return path;
        }
    }
    return "log.txt";
}

void saveLastFilePath(const std::string& path) {
    std::ofstream config(getConfigPath());
    if (config.is_open()) {
        config << path;
        config.close();
    }
}

// =============================================================================
// Clipboard functionality
// =============================================================================

bool copyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    
    EmptyClipboard();
    
    size_t size = (text.length() + 1) * sizeof(char);
    HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hClipboardData) {
        CloseClipboard();
        return false;
    }
    
    char* pchData = static_cast<char*>(GlobalLock(hClipboardData));
    if (!pchData) {
        GlobalFree(hClipboardData);
        CloseClipboard();
        return false;
    }
    
    strcpy_s(pchData, size, text.c_str());
    GlobalUnlock(hClipboardData);
    
    SetClipboardData(CF_TEXT, hClipboardData);
    CloseClipboard();
    return true;
#else
    // Linux: Use xclip if available, otherwise try xsel
    std::string command = "echo '" + text + "' | xclip -selection clipboard 2>/dev/null || echo '" + text + "' | xsel --clipboard --input 2>/dev/null";
    return system(command.c_str()) == 0;
#endif
}

// =============================================================================
// Helper functions for log display
// =============================================================================

std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:  return " DEBUG";
        case LogLevel::INFO:   return "  INFO";
        case LogLevel::WARN:   return "  WARN";
        case LogLevel::ERROR:  return " ERROR";
        case LogLevel::FOOTER: return "FOOTER";
        case LogLevel::HEADER: return "HEADER";
        default:               return " DEBUG";
    }
}

Color LogLevelToColor(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:  return Color::Cyan;
        case LogLevel::INFO:   return Color::Green;
        case LogLevel::WARN:   return Color::Yellow;
        case LogLevel::ERROR:  return Color::Red;
        case LogLevel::FOOTER: return Color::Blue;
        case LogLevel::HEADER: return Color::Blue;
        default:               return Color::Cyan;
    }
}

// =============================================================================
// Main application
// =============================================================================

int main() {
#ifdef _WIN32
    enableWindowsConsoleColors();
#endif

    auto screen = ScreenInteractive::Fullscreen();

    // -------------------------------------------------------------------------
    // Application state
    // -------------------------------------------------------------------------
    std::vector<LogEntry> log_entries;
    std::mutex log_entries_mutex;
    std::string input_file_path = loadLastFilePath();
    std::string search_term;
    std::string status_message = "Ready";
    int scroll_y = 0;
    
    // Filter states
    bool show_debug = false;
    bool show_info = false;
    bool show_warn = false;
    bool show_error = false;

    // -------------------------------------------------------------------------
    // Create UI components
    // -------------------------------------------------------------------------
    auto input_file = Input(&input_file_path, "path/to/log.txt");
    auto input_search = Input(&search_term, "search term");
    
    // Create parser instance
    LogParser parser;
    
    auto parse_button = Button("Open", [&] {
        // Clear existing entries
        {
            std::lock_guard<std::mutex> lock(log_entries_mutex);
            log_entries.clear();
        }
        scroll_y = 0;
        
        // Start async parsing with progress callback
        parser.parseAsync(input_file_path, log_entries, log_entries_mutex, 
                         [&](const std::string& progress) {
                             status_message = progress;
                             // Force UI refresh
                             screen.PostEvent(Event::Custom);
                         });
        
        saveLastFilePath(input_file_path);
        input_search->TakeFocus();
    });
    
    auto copy_button = Button("Copy Filtered", [&] {
        // Get filtered entries (same logic as in log renderer)
        std::vector<LogEntry> filtered_entries;
        bool any_filter_checked = show_debug || show_info || show_warn || show_error;
        
        // Thread-safe access to log entries
        std::vector<LogEntry> local_entries;
        {
            std::lock_guard<std::mutex> lock(log_entries_mutex);
            local_entries = log_entries;
        }
        
        for (const auto& entry : local_entries) {
            // Apply level filters
            bool level_matches = !any_filter_checked || 
                (show_debug && entry.level == LogLevel::DEBUG) ||
                (show_info && entry.level == LogLevel::INFO) ||
                (show_warn && entry.level == LogLevel::WARN) ||
                (show_error && entry.level == LogLevel::ERROR);
            
            if (!level_matches) continue;
            
            // Apply search filter
            if (!search_term.empty() && 
                entry.message.find(search_term) == std::string::npos) {
                continue;
            }
            
            filtered_entries.push_back(entry);
        }
        
        // Build clipboard text
        std::string clipboard_text;
        for (const auto& entry : filtered_entries) {
            std::string source_info = entry.source_file + ":" + std::to_string(entry.source_line);
            clipboard_text += "[" + entry.timestamp + "][" + LogLevelToString(entry.level) + "]: " + 
                             entry.message + " | " + source_info + "\n";
        }
        
        if (copyToClipboard(clipboard_text)) {
            status_message = "Copied " + std::to_string(filtered_entries.size()) + " entries to clipboard";
        } else {
            status_message = "Failed to copy to clipboard";
        }
        screen.PostEvent(Event::Custom);
    });
    
    auto checkbox_debug = Checkbox("DEBUG", &show_debug);
    auto checkbox_info = Checkbox("INFO", &show_info);
    auto checkbox_warn = Checkbox("WARN", &show_warn);
    auto checkbox_error = Checkbox("ERROR", &show_error);

    // -------------------------------------------------------------------------
    // Container structure
    // -------------------------------------------------------------------------
    
    // File controls: Input field, Open button, and Copy button
    auto file_controls_container = Container::Horizontal({
        input_file,
        parse_button,
        copy_button,
    });

    // Search and filter controls
    auto search_filters_container = Container::Vertical({
        input_search,
        Container::Horizontal({
            checkbox_debug,
            checkbox_info,
            checkbox_warn,
            checkbox_error,
        }),
    });

    // Log display container with scrolling support
    auto log_display_container = Container::Vertical({}) | CatchEvent([&](Event event) {
        if (event == Event::ArrowUp && scroll_y > 0) {
            scroll_y--;
            return true;
        }
        if (event == Event::ArrowDown) {
            scroll_y++;
            return true;
        }
        if (event.is_mouse()) {
            if (event.mouse().button == Mouse::WheelUp && scroll_y > 0) {
                scroll_y--;
                return true;
            }
            if (event.mouse().button == Mouse::WheelDown) {
                scroll_y++;
                return true;
            }
        }
        return false;
    });

    // Main container hierarchy
    auto main_container = Container::Vertical({
        file_controls_container,
        log_display_container,
        search_filters_container,
    });

    // -------------------------------------------------------------------------
    // Renderers for each section
    // -------------------------------------------------------------------------
    
    // File controls renderer (top pane)
    auto file_renderer = Renderer(file_controls_container, [&] {
        // Left side: File and Status stacked vertically
        auto left_section = vbox({
            hbox({
                text("File: "),
                input_file->Render() | flex,
            }),
            text(""),  // Padding
            hbox({
                text("Status: "),
                text(status_message) | color(Color::Green) | flex,
            }),
        });
        
        // Right side: Open and Copy buttons
        auto right_section = vbox({
            filler(),
            hbox({
                parse_button->Render(),
                text("  "),
                copy_button->Render(),
            }),
            filler(),
        });
        
        return hbox({
            left_section | flex,
            separator(),
            right_section,
        }) | border;
    });
    
    // Log display renderer (center pane)
    auto log_renderer = Renderer(log_display_container, [&] {
        // Filter log entries (but only store indices to avoid copying)
        std::vector<size_t> filtered_indices;
        bool any_filter_checked = show_debug || show_info || show_warn || show_error;
        
        // Thread-safe access to log entries
        std::vector<LogEntry> local_entries;
        {
            std::lock_guard<std::mutex> lock(log_entries_mutex);
            local_entries = log_entries;  // Make a copy for processing
        }
        
        for (size_t i = 0; i < local_entries.size(); ++i) {
            const auto& entry = local_entries[i];
            // Apply level filters
            bool level_matches = !any_filter_checked || 
                (show_debug && entry.level == LogLevel::DEBUG) ||
                (show_info && entry.level == LogLevel::INFO) ||
                (show_warn && entry.level == LogLevel::WARN) ||
                (show_error && entry.level == LogLevel::ERROR);
            
            if (!level_matches) continue;
            
            // Apply search filter
            if (!search_term.empty() && 
                entry.message.find(search_term) == std::string::npos) {
                continue;
            }
            
            filtered_indices.push_back(i);
        }

        // Create header
        Elements header_cells;
        header_cells.push_back(text("Timestamp") | bold | color(Color::RGB(255, 215, 0)) | center | size(WIDTH, EQUAL, 15));
        header_cells.push_back(separator());
        header_cells.push_back(text("Level") | bold | color(Color::RGB(255, 215, 0)) | center | size(WIDTH, EQUAL, 10));
        header_cells.push_back(separator());
        header_cells.push_back(text("Message") | bold | color(Color::RGB(255, 215, 0)) | flex);
        header_cells.push_back(separator());
        header_cells.push_back(text("Source") | bold | color(Color::RGB(255, 215, 0)) | size(WIDTH, EQUAL, 50));
        auto header = hbox(std::move(header_cells));

        // Virtualization: only render visible rows (max 45 to fit in our height limit)
        const int max_visible_rows = 45;
        const int total_filtered = filtered_indices.size();
        
        // Clamp scroll position
        if (scroll_y >= total_filtered) {
            scroll_y = std::max(0, total_filtered - max_visible_rows);
        }
        if (scroll_y < 0) scroll_y = 0;
        
        // Calculate visible range
        int start_idx = scroll_y;
        int end_idx = std::min(start_idx + max_visible_rows, total_filtered);
        
        // Create log rows only for visible entries
        Elements log_rows;
        for (int i = start_idx; i < end_idx; ++i) {
            const auto& entry = local_entries[filtered_indices[i]];
            std::string source_info = entry.source_file + ":" + std::to_string(entry.source_line);
            
            auto log_row = hbox({
                text(entry.timestamp) | size(WIDTH, EQUAL, 15),
                separator(),
                text(LogLevelToString(entry.level)) | color(LogLevelToColor(entry.level)) | size(WIDTH, EQUAL, 10),
                separator(),
                text(entry.message) | flex,
                separator(),
                text(source_info) | size(WIDTH, EQUAL, 50) | dim,
            });
            log_rows.push_back(log_row);
        }
        
        Element log_content = log_rows.empty() 
            ? text("No log entries to display") | center | dim
            : vbox(std::move(log_rows)) | vscroll_indicator | frame;

        std::string log_status = "Showing " + std::to_string(total_filtered) + 
                               " of " + std::to_string(local_entries.size()) + " entries";
        
        // Add scroll position indicator for large datasets
        std::string scroll_info = "";
        if (total_filtered > max_visible_rows) {
            int scroll_percentage = total_filtered > 0 ? (scroll_y * 100) / std::max(1, total_filtered - max_visible_rows) : 0;
            scroll_info = " | Scroll: " + std::to_string(scroll_percentage) + "%";
        }

        return vbox({
            hbox({
                filler(),
                text(log_status + scroll_info) | dim,
            }),
            separator(),
            header,
            separator(),
            log_content | flex,
        }) | border;
    });

    // Search and filters renderer (bottom pane)
    auto search_renderer = Renderer(search_filters_container, [&] {
        return vbox({
            hbox({
                text("Search: ") | size(WIDTH, EQUAL, 8),
                input_search->Render() | flex,
            }),
            separator(),
            hbox({
                text("Filters: ") | size(WIDTH, EQUAL, 8),
                checkbox_debug->Render(),
                text(" "),
                checkbox_info->Render(),
                text(" "),
                checkbox_warn->Render(),
                text(" "),
                checkbox_error->Render(),
            }),
        }) | border;
    });

    // -------------------------------------------------------------------------
    // Main layout composition
    // -------------------------------------------------------------------------
    
    auto main_renderer = Container::Vertical({
        file_renderer,
        log_renderer,
        search_renderer,
    });

    auto final_renderer = Renderer(main_renderer, [&] {
        return vbox({
            file_renderer->Render() | size(HEIGHT, EQUAL, 5),
            separator(),
            log_renderer->Render() | size(HEIGHT, EQUAL, 50),
            separator(),
            search_renderer->Render() | size(HEIGHT, EQUAL, 6),
        });
    });

    // Add escape key handling
    auto escape_handler = CatchEvent(final_renderer, [&](Event event) {
        if (event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    // -------------------------------------------------------------------------
    // Run the application
    // -------------------------------------------------------------------------
    screen.Loop(escape_handler);
    
    // Clean up parser thread before exit
    parser.stopParsing();
    return 0;
}