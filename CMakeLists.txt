cmake_minimum_required(VERSION 3.24)
project(napkin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Test)
find_package(SQLite3 REQUIRED)

qt_standard_project_setup()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- core ---------------------------------------------------------------------
# Domain and data layers. Links Qt6::Core but deliberately NOT Qt6::Widgets, so
# the "domain has no GUI dependency" rule (SPEC.md §10) is enforced by the build
# rather than by good intentions. Everything here is testable headless.
add_library(napkin_core STATIC
  src/domain/Clock.cpp
  src/domain/BufferService.cpp
  src/data/Database.cpp
  src/data/Statement.cpp
  src/data/Migrations.cpp
  src/data/BufferRepository.cpp
  src/data/ItemRepository.cpp
  src/app/Paths.cpp
)
target_link_libraries(napkin_core PUBLIC Qt6::Core SQLite3::SQLite3)
target_include_directories(napkin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- application --------------------------------------------------------------
qt_add_executable(napkin
  src/main.cpp
  src/ui/MainWindow.cpp
  src/app/SingleInstance.cpp
)
target_link_libraries(napkin PRIVATE napkin_core Qt6::Widgets Qt6::Network)
set_target_properties(napkin PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)

# --- Phase 0 spike (disposable) -----------------------------------------------
qt_add_executable(napkin_spike EXCLUDE_FROM_ALL src/spike/main.cpp)
target_link_libraries(napkin_spike PRIVATE Qt6::Widgets SQLite3::SQLite3)

# --- tests --------------------------------------------------------------------
enable_testing()
foreach(t test_migrations test_invariants test_buffers test_items test_paths)
  qt_add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE napkin_core Qt6::Test)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
