# LogReader

A cross-platform terminal-based log file viewer with real-time filtering and search capabilities.

## Features

- **Fast Log Parsing**: Efficiently parses structured log files with regex
- **Real-time Filtering**: Filter by log levels (DEBUG, INFO, WARN, ERROR) without re-parsing
- **Search Functionality**: Search through log messages instantly
- **Scrollable Display**: Navigate through large log files with keyboard and mouse
- **Cross-Platform**: Works on Windows, Linux, and macOS
- **Beautiful TUI**: Built with FTXUI for a modern terminal interface
- **Persistent Configuration**: Remembers your last opened file

## Log Format

The parser expects log entries in this format:
```
[timestamp][LEVEL]: message | source_file -> function(): line_number
```

Example:
```
[15:21:49.586][ DEBUG]: Swapchain created with 2 images. | Helix.cpp -> initSwapchain(): 421
[15:21:49.638][HEADER]: >>>>>>>> Creating RenderOrchestrator | Helix.cpp -> init(): 196
```

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
   - Level filter checkboxes

## Dependencies

- **FTXUI v5.0.0**: Terminal UI library (fetched automatically via CMake)

## License

[Add your license here]