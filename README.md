# LogReader

A cross-platform terminal-based log file viewer with real-time filtering and search capabilities.

## Features

- **Fast Log Parsing**: Efficiently parses structured log files using ASCII field separators
- **Real-time Filtering**: Filter by log levels (DEBUG, INFO, WARN, ERROR) without re-parsing
- **Search Functionality**: Search through log messages instantly
- **Scrollable Display**: Navigate through large log files with keyboard and mouse
- **Cross-Platform**: Works on Windows, Linux, and macOS
- **Beautiful TUI**: Built with FTXUI for a modern terminal interface
- **Persistent Configuration**: Remembers your last opened file

## Log Format

The parser expects log entries using ASCII field separators (character 31) in this format:
```
timestamp<FS>LEVEL<FS>message<FS>source_file -> function(): line_number
```

Where `<FS>` represents the ASCII field separator (char 31).

Example:
```
16:29:40.318<FS>DEBUG<FS>Vulkan loader version: 1.4.304<FS>Vulkan.cpp -> initVulkan(): 92
16:29:40.587<FS>INFO<FS>Supported instance extensions:<FS>Vulkan.cpp -> initVulkan(): 106
```

### Benefits of ASCII Field Separator Format

- **Faster parsing**: Simple field splitting vs complex regex matching
- **More reliable**: Eliminates ambiguity when log messages contain special characters
- **Better performance**: Handles large log files more efficiently
- **Cleaner data**: No escaping needed for brackets, colons, or pipes in log messages

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler
- Git (for fetching dependencies)

### Windows (Visual Studio)

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux/macOS

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

Run the application:
```bash
./log_reader        # Linux/macOS
log_reader.exe      # Windows
```

### Controls

- **Tab**: Navigate between controls
- **Enter**: Open log file
- **Arrow Keys**: Scroll through log entries
- **Mouse Wheel**: Scroll through log entries
- **Copy Filtered**: Copy currently filtered/searched entries to clipboard
- **Escape**: Exit application

### Interface Layout

1. **File Controls** (Top): 
   - File path input
   - Status display
   - Open button

2. **Log Display** (Center):
   - Filtered log entries with columns: Timestamp, Level, Message, Source
   - Shows count of displayed vs total entries
   - Scrollable view

3. **Search & Filters** (Bottom):
   - Search text input
   - Level filter checkboxes (DEBUG, INFO, WARN, ERROR, HEADER, FOOTER)
   - Copy filtered results to clipboard

## Dependencies

- **FTXUI v5.0.0**: Terminal UI library (fetched automatically via CMake)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.