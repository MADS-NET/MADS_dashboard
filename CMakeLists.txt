cmake_minimum_required(VERSION 3.28)

# Release by default (must precede project(); a Debug build on MSVC cannot
# share std::string/json objects with a Release MadsCore). Only when this is
# the top-level project: inside the MADS build, MADS decides.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND
   NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build.")
endif()

project(mads_dashboard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Two ways to build:
#  - standalone (top-level project) against an installed MADS (find_package);
#  - inside the MADS build, pulled in with FetchContent: the in-tree
#    Mads::Mads alias and MadsMainDeps (MADS's header deps) are used, and only
#    the executable is installed, into MADS_DASH_INSTALL_COMPONENT.
option(MADS_DASH_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(MADS_DASH_BUILD_TESTSOURCE "Build tools/testsource (testsource.plugin)"
       ${PROJECT_IS_TOP_LEVEL})
set(MADS_DASH_INSTALL_COMPONENT "MadsDashboard" CACHE STRING
    "Install component of the mads-dashboard executable (MADS: MadsGui)")

# MADS #########################################################################
if(TARGET Mads::Mads)
  set(MADS_DASH_IN_MADS_TREE ON)
  message(STATUS "mads-dashboard: building inside the MADS project")
else()
  set(MADS_DASH_IN_MADS_TREE OFF)
  # Locate the MADS install prefix through the `mads` CLI unless given.
  if(NOT Mads_DIR)
    execute_process(COMMAND mads -p
      OUTPUT_VARIABLE MADS_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(MADS_PREFIX)
      list(APPEND CMAKE_PREFIX_PATH "${MADS_PREFIX}")
    endif()
  endif()
  find_package(Mads REQUIRED)
  get_target_property(MADS_INCLUDE_DIR Mads::Mads INTERFACE_INCLUDE_DIRECTORIES)
  list(GET MADS_INCLUDE_DIR 0 MADS_INCLUDE_DIR)
  cmake_path(GET MADS_INCLUDE_DIR PARENT_PATH MADS_PREFIX)
  message(STATUS "MADS prefix: ${MADS_PREFIX}")
  # standalone installs default to the MADS prefix, next to the mads CLI
  if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${MADS_PREFIX}" CACHE PATH "" FORCE)
  endif()
endif()
include(GNUInstallDirs)

# Third-party dependencies (static, via FetchContent) ##########################
include(cmake/Dependencies.cmake)

# Core library (everything except main), so tests can link it ##################
add_library(mads_dash_core STATIC
  src/app/version.cpp
  src/data/store.cpp
  src/data/export_hdf5.cpp
  src/net/receiver.cpp
)
target_include_directories(mads_dash_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(mads_dash_core PUBLIC Mads::Mads)
if(MADS_DASH_IN_MADS_TREE)
  set(_dash_prefix "${CMAKE_INSTALL_PREFIX}")
else()
  set(_dash_prefix "${MADS_PREFIX}")
endif()
set(_dash_keys_dir "${_dash_prefix}/etc")
target_compile_definitions(mads_dash_core PRIVATE
  MADS_DASH_DEFAULT_KEYS_DIR="${_dash_keys_dir}")
if(TARGET MadsMainDeps) # in-tree MADS: its header deps are not on Mads::Mads
  target_link_libraries(mads_dash_core PUBLIC MadsMainDeps)
endif()
target_link_libraries(mads_dash_core PRIVATE dash_hdf5)
configure_file(src/app/version.hpp.in generated/app/version.hpp @ONLY)

if(MSVC)
  target_compile_options(mads_dash_core PRIVATE /W4)
else()
  target_compile_options(mads_dash_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GUI library ##################################################################
# UI sources (src/ui/*.cpp, one file pair per chart type in src/ui/views/)
# are globbed so adding a file needs no CMake edit. im_assert.cpp belongs to
# dash_imgui (cmake/Dependencies.cmake). The script host (src/script) is here
# because its `dash.fft` helper reuses the Spectrum view's FFT.
file(GLOB UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp src/ui/views/*.cpp
  src/script/*.cpp)
list(REMOVE_ITEM UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/im_assert.cpp)
add_library(mads_dash_ui STATIC ${UI_SOURCES})
# Background logo of the Views panel: raw RGBA (see tools/logo_to_rgba.py)
# turned into a C array initializer at configure time.
set(_logo ${CMAKE_CURRENT_SOURCE_DIR}/assets/mads_logo.rgba)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_logo})
file(READ ${_logo} _logo_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," _logo_hex "${_logo_hex}")
file(CONFIGURE OUTPUT generated/ui/mads_logo.inc CONTENT "${_logo_hex}")
target_link_libraries(mads_dash_ui PUBLIC mads_dash_core dash_imgui)
target_link_libraries(mads_dash_ui PRIVATE dash_lua)
# Default scripts folder, where the example scripts are installed.
target_compile_definitions(mads_dash_ui PRIVATE
  MADS_DASH_DEFAULT_SCRIPTS_DIR="${_dash_prefix}/${CMAKE_INSTALL_DATADIR}/mads-dashboard/scripts")

# Executable ###################################################################
add_executable(mads-dashboard src/app/main.cpp)
target_link_libraries(mads-dashboard PRIVATE mads_dash_ui dash_hdf5)

# The only runtime dependency is libMadsCore, found relative to the installed
# executable (<prefix>/bin -> <prefix>/lib, as the MADS GUI apps do); Windows
# finds MadsCore.dll in <prefix>/bin, next to the exe.
if(APPLE)
  set(_dash_rel_rpath "@loader_path/../lib")
elseif(UNIX)
  set(_dash_rel_rpath "\$ORIGIN/../lib")
endif()
if(DEFINED _dash_rel_rpath)
  set_target_properties(mads-dashboard PROPERTIES
    INSTALL_RPATH "${_dash_rel_rpath}" INSTALL_RPATH_USE_LINK_PATH FALSE)
  if(NOT MADS_DASH_IN_MADS_TREE) # run from the build tree, too
    set_target_properties(mads-dashboard PROPERTIES
      BUILD_RPATH "${MADS_PREFIX}/lib")
  endif()
endif()

install(TARGETS mads-dashboard
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT ${MADS_DASH_INSTALL_COMPONENT})
install(DIRECTORY share/scripts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mads-dashboard/scripts
  COMPONENT ${MADS_DASH_INSTALL_COMPONENT} FILES_MATCHING PATTERN "*.lua")

# Tests ########################################################################
if(MADS_DASH_BUILD_TESTS)
  enable_testing() # adds tests to this directory and below only
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
