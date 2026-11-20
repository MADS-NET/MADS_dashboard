cmake_minimum_required(VERSION 3.28)

# Release by default (must precede project(); a Debug build on MSVC cannot
# share std::string/json objects with a Release MadsCore).
if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build.")
endif()

project(mads_dashboard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MADS_DASH_BUILD_TESTS "Build unit tests" ON)
option(MADS_DASH_BUILD_TESTSOURCE "Build tools/testsource (testsource.plugin)" ON)

# MADS (installed, required) ###################################################
# Locate the MADS install prefix through the `mads` CLI unless the user gave one.
if(NOT Mads_DIR)
  execute_process(COMMAND mads -p
    OUTPUT_VARIABLE MADS_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(MADS_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${MADS_PREFIX}")
  endif()
endif()
find_package(Mads REQUIRED)
get_target_property(MADS_INCLUDE_DIR Mads::Mads INTERFACE_INCLUDE_DIRECTORIES)
cmake_path(GET MADS_INCLUDE_DIR PARENT_PATH MADS_PREFIX)
message(STATUS "MADS prefix: ${MADS_PREFIX}")

# Third-party dependencies (static, via FetchContent) ##########################
include(cmake/Dependencies.cmake)

# Core library (everything except main), so tests can link it ##################
add_library(mads_dash_core STATIC
  src/app/version.cpp
  src/data/store.cpp
  src/net/receiver.cpp
)
target_include_directories(mads_dash_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(mads_dash_core PUBLIC Mads::Mads)
configure_file(src/app/version.hpp.in generated/app/version.hpp @ONLY)

if(MSVC)
  target_compile_options(mads_dash_core PRIVATE /W4)
else()
  target_compile_options(mads_dash_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GUI library ##################################################################
# One file pair per chart type in src/ui/views/; globbed so adding a view
# needs no CMake edit.
file(GLOB VIEW_SOURCES CONFIGURE_DEPENDS src/ui/views/*.cpp)
add_library(mads_dash_ui STATIC
  src/ui/app_window.cpp
  src/ui/view.cpp
  ${VIEW_SOURCES}
)
target_link_libraries(mads_dash_ui PUBLIC mads_dash_core dash_imgui)

# Executable ###################################################################
add_executable(mads-dashboard src/app/main.cpp)
target_link_libraries(mads-dashboard PRIVATE mads_dash_ui dash_hdf5)

# The only runtime dependency is the MADS install's libMadsCore.
if(APPLE)
  set_target_properties(mads-dashboard PROPERTIES
    BUILD_RPATH "${MADS_PREFIX}/lib"
    INSTALL_RPATH "@executable_path/../lib;${MADS_PREFIX}/lib")
elseif(UNIX)
  set_target_properties(mads-dashboard PROPERTIES
    BUILD_RPATH "${MADS_PREFIX}/lib"
    INSTALL_RPATH "\$ORIGIN/../lib;${MADS_PREFIX}/lib")
endif()

install(TARGETS mads-dashboard RUNTIME DESTINATION bin)

# Tests ########################################################################
if(MADS_DASH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Test source plugin (separate CMake project, built with the plugin scaffold)
if(MADS_DASH_BUILD_TESTSOURCE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/testsource/CMakeLists.txt)
  include(ExternalProject)
  if(POLICY CMP0225) # CMake >= 4.1
    cmake_policy(SET CMP0225 NEW)
  endif()
  ExternalProject_Add(testsource
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/testsource
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/testsource
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
  )
endif()
