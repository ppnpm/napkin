cmake_minimum_required(VERSION 3.24)
project(napkin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Widgets Network Test)
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
  src/domain/Preview.cpp
  src/domain/TimeFormat.cpp
  src/data/Database.cpp
  src/data/Statement.cpp
  src/data/Migrations.cpp
  src/data/BufferRepository.cpp
  src/data/ItemRepository.cpp
  src/app/Paths.cpp
)
target_link_libraries(napkin_core PUBLIC Qt6::Core SQLite3::SQLite3)
target_include_directories(napkin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- media --------------------------------------------------------------------
# Image handling needs QImage, which lives in Qt6::Gui. Kept in its own library
# so napkin_core stays Core-only: the domain and data layers must not acquire a
# GUI dependency by proximity.
add_library(napkin_media STATIC
  src/media/BlobStore.cpp
  src/media/ImageFormats.cpp
  src/media/BlobGc.cpp
  src/media/Thumbnailer.cpp
  src/media/ClipboardContent.cpp
)
target_link_libraries(napkin_media PUBLIC napkin_core Qt6::Gui)

# --- ui -----------------------------------------------------------------------
# A library rather than loose sources in the executable, so the widget tree can
# be driven headlessly by tests instead of only by hand.
qt_add_library(napkin_ui STATIC
  src/ui/MainWindow.cpp
  src/ui/BufferListModel.cpp
  src/ui/BufferListView.cpp
  src/ui/BufferCardDelegate.cpp
  src/ui/BufferEditor.cpp
  src/ui/ItemWidgets.cpp
  src/ui/Autosave.cpp
  src/ui/Icons.cpp
  src/ui/UndoToast.cpp
  src/ui/Lightbox.cpp
  src/app/SingleInstance.cpp
)
target_link_libraries(napkin_ui PUBLIC napkin_core napkin_media Qt6::Widgets Qt6::Network)

# --- application --------------------------------------------------------------
qt_add_executable(napkin src/main.cpp)
target_link_libraries(napkin PRIVATE napkin_ui)
set_target_properties(napkin PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)

# --- Phase 0 spike (disposable) -----------------------------------------------
qt_add_executable(napkin_spike EXCLUDE_FROM_ALL src/spike/main.cpp)
target_link_libraries(napkin_spike PRIVATE Qt6::Widgets SQLite3::SQLite3)

# --- tests --------------------------------------------------------------------
enable_testing()
foreach(t test_migrations test_invariants test_buffers test_items test_paths test_preview test_performance)
  qt_add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE napkin_core Qt6::Test)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# GUI tests drive the real widget tree offscreen.
foreach(t test_editing test_lifecycle test_images test_multiitem)
  qt_add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE napkin_ui Qt6::Test)
  add_test(NAME ${t} COMMAND ${t})
  set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
