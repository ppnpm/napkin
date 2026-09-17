cmake_minimum_required(VERSION 3.24)
project(napkin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
find_package(SQLite3 REQUIRED)

qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Phase 0 spike -----------------------------------------------------------
# Disposable. Exists only to retire platform risk before Phase 1 begins:
#   1. clipboard image paste on Wayland/KDE, from Spectacle and from a browser
#   2. SQLite WAL survives kill -9
#   3. the CMake + Qt6 + sqlite3 toolchain produces a runnable artifact
qt_add_executable(napkin_spike src/spike/main.cpp)
target_link_libraries(napkin_spike PRIVATE Qt6::Widgets SQLite3::SQLite3)
set_target_properties(napkin_spike PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)
