# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Linux/macOS Build
Build the project using CMake:
```bash
# Configure and build from project root
mkdir -p build && cd build
cmake ..
make

# Run the application
./log_reader
```

### Windows Build
Build the project using CMake with Visual Studio or MinGW:

**Using Visual Studio (recommended):**
```cmd
# Configure and build from project root
mkdir build
cd build
cmake ..
cmake --build . --config Release

# Run the application
Release\log_reader.exe
```

**Using MinGW:**
```bash
# Configure and build from project root
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make

# Run the application
log_reader.exe
```

**Prerequisites for Windows:**
- CMake 3.16 or higher
- Visual Studio 2019+ with C++ tools, or MinGW-w64
- Git (for fetching FTXUI dependency)
- Windows 10+ for optimal ANSI color support

The project builds to `build/Release/log_reader.exe` (Visual Studio) or `build/log_reader.exe` (MinGW).

## Architecture Overview

This is a C++ log file reader and viewer with a terminal-based user interface. The application parses structured log files and provides interactive filtering and search capabilities.

### Core Components

- **LogEntry.hpp**: Defines the `LogEntry` struct and `LogLevel` enum for representing parsed log entries
- **LogParser**: Handles parsing log files using regex to extract structured data (timestamp, level, message, source file, function, line number)
- **main.cpp**: Contains the FTXUI-based terminal interface with real-time filtering and search

### Log Format

The parser expects log entries in this format:
```
[timestamp][LEVEL]: message | source_file -> function(): line_number
```

Examples:
```
[15:21:49.586][ DEBUG]: Swapchain created with 2 images. | Helix.cpp -> initSwapchain(): 421
[15:21:49.638][HEADER]: >>>>>>>> Creating RenderOrchestrator | Helix.cpp -> init(): 196
```

### UI Architecture

Built with FTXUI library (v5.0.0) using a component-based architecture:
- **LogEntryComponent**: Renders individual log entries with fixed-width columns
- **Controls**: File input, search field, level filter checkboxes, parse button
- **Log View**: Scrollable table with header showing filtered/searched entries

The UI provides real-time filtering by log level and text search without requiring re-parsing.

### Dependencies

- C++17 standard
- FTXUI v5.0.0 (fetched automatically via CMake FetchContent)
- Standard library: regex, fstream, string, vector

### Sample Data

The project includes `log.txt` with sample Vulkan/graphics engine log entries for testing the parser and UI.