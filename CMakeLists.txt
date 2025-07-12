cmake_minimum_required(VERSION 3.16)
project(LogReader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Unicode support on Windows
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    # Enable console colors on Windows 10+
    add_definitions(-DNOMINMAX)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

include_directories(src)

add_executable(log_reader 
    src/main.cpp 
    src/LogParser.cpp
)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
)

FetchContent_MakeAvailable(ftxui)

target_link_libraries(log_reader PRIVATE ftxui::screen ftxui::dom ftxui::component)
